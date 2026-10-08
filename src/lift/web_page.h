#pragma once
#include <Arduino.h>

const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="ru"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Лифт · Настройки</title>
<style>
body{font:16px system-ui;background:#101826;color:#edf3ff;margin:0;padding:24px}main{max-width:760px;margin:auto}h1{margin-bottom:8px}.muted{color:#aabbd4}section{background:#1c293c;border-radius:16px;padding:24px;margin:20px 0}dl{display:grid;grid-template-columns:1fr 1fr;gap:14px}dd{margin:0;text-align:right}button:disabled{opacity:.45;cursor:default}button{background:#73d5b6;color:#10251f;border:0;border-radius:8px;padding:14px;font:inherit;cursor:pointer}.buttons{display:flex;flex-wrap:wrap;gap:10px}.stop{background:#ff8585}.secondary{background:#b2c5df}#down{touch-action:none;user-select:none}a{color:#73d5b6}
</style>
<main><p class="muted">ELEVATOR ESP32</p><h1>Состояние лифта</h1><p id="connection">Подключение…</p>
<section><dl id="values"></dl></section>
<section><h2>Калибровка</h2><p>1. Найдите верхний концевик. 2. Удерживайте спуск до нижней точки. 3. Отпустите кнопку, дождитесь остановки и сохраните низ.</p>
<p id="calibState">Ожидание данных</p><div class="buttons">
<button id="start" disabled>Найти верх</button><button id="down" disabled>Удерживать: вниз</button>
<button id="save" disabled>Сохранить низ</button><button id="stop" class="stop" disabled>СТОП</button>
<button id="reset" class="secondary" disabled>Сбросить калибровку</button></div>
<p id="calibMessage"></p><p class="muted">При уходе со страницы или потере связи веб-калибровка останавливается. Веб-СТОП не заменяет физическую аварийную кнопку. Удерживающий момент мотора сохраняется.</p></section>
<section><h2>Подключение к Wi-Fi</h2><p>Смена сети доступна после остановки, вне калибровки.</p>
<button id="network" disabled>Настроить другую сеть</button><p id="message"></p></section></main>
<script>
const states=['Запуск','Нужна калибровка','Калибровка вверх','Калибровка вниз','Ожидание','Поездка','Ручное движение','Ошибка','Нужно найти верх','Поиск верха'];
const results=['','Поиск верхней точки запущен','Спуск приостановлен','Калибровка сохранена','Калибровка сброшена','Остановлено: потеря связи с браузером','Команда отклонена: проверьте состояние','Остановлено командой STOP','Ошибка контроллера — калибровка прервана'];
const owner=crypto.getRandomValues(new Uint32Array(1))[0]||1;
let token='',snapshot=null,sequence=0,session=false,generation=0,heldDown=false,awaitingStart=0,commandBusy=false,heartbeatTimer=null;
const el=id=>document.getElementById(id);
function controls(){
  const live=!!token&&!!snapshot, mine=live&&snapshot.calibOwner===owner;
  el('start').disabled=!live||!snapshot.calibCanStart||session||commandBusy;
  el('down').disabled=!mine||!session||snapshot.state!==3;
  el('save').disabled=!mine||!session||!snapshot.calibCanSave||heldDown||commandBusy;
  el('reset').disabled=!live||!snapshot.calibCanStart||session||commandBusy;
  el('stop').disabled=!token;
  el('network').disabled=!live||!snapshot.stationary||!!snapshot.calibOwner||snapshot.networkResult===1||session;
}
async function sendAction(action){
  if(!token)throw Error('Нет связи');
  const body=new URLSearchParams({action,owner:String(owner),generation:String(session?generation:snapshot.calibGeneration),sequence:String(++sequence)});
  const r=await fetch('/api/calibration',{method:'POST',headers:{'X-Lift-Token':token},body,signal:AbortSignal.timeout(300)});
  if(!r.ok)throw Error(await r.text());
}
function stopSession(send=true){
  session=false;heldDown=false;awaitingStart=0;clearTimeout(heartbeatTimer);heartbeatTimer=null;
  if(send&&token)fetch('/api/stop',{method:'POST',headers:{'X-Lift-Token':token},keepalive:true}).catch(()=>{});
  controls();
}
async function heartbeat(){
  if(!session)return;
  try{await sendAction(heldDown?'down':'heartbeat');}
  catch(e){el('calibMessage').textContent='Нет подтверждения команды. Сессия остановлена.';stopSession();return;}
  if(session)heartbeatTimer=setTimeout(heartbeat,150);
}
async function update(){
  try{
    const r=await fetch('/api/status',{cache:'no-store',signal:AbortSignal.timeout(1200)});
    if(!r.ok)throw Error();const s=await r.json();if(s.age>1000)throw Error('stale');
    token=s.token;snapshot=s;
    if(session){
      if(s.calibGeneration!==generation||(s.calibOwner&&s.calibOwner!==owner))stopSession(false);
      else if(s.calibOwner===owner)awaitingStart=0;
      else if(!awaitingStart||Date.now()-awaitingStart>1000)stopSession();
    }
    if(s.networkResult===2)el('message').textContent='Смена сети отменена: лифт начал движение или запрос устарел';
    el('connection').textContent='База доступна · '+s.ip;
    const phase=s.calibOwner&&s.state===2?'Подъём до верхнего концевика':s.calibOwner&&s.state===3?(s.running?'Спуск. Отпустите кнопку для остановки':'Верх найден. Удерживайте вниз или сохраните нижнюю точку'):(results[s.calibResult]||states[s.state]);
    el('calibState').textContent=(s.calibOwner&&s.calibOwner!==owner?'Управляет другая вкладка. ':'')+phase;
    const rows=[['Состояние',states[s.state]||s.state],['Этаж / цель',s.floor+' / '+s.target],['Позиция, шагов',s.position],['Позиция известна',s.known?'Да':'Нет'],['Ошибка',s.error],['Верхний концевик',s.top?'Нажат':'Свободен'],['Регулятор скорости',s.speed+'%'],['Пульт зарегистрирован',s.peer?'Да':'Нет'],['Радиоканал',s.channel],['Время работы, с',Math.floor(s.uptime/1000)],['Мотор',s.running?'Движется':'Остановлен'],['Ход, шагов',s.travel],['Свободная память, КБ',Math.round(s.freeHeap/1024)],['Сигнал Wi-Fi',s.rssi+' dBm']];
    const box=el('values');box.replaceChildren();for(const [k,v]of rows){const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=k;dd.textContent=v;box.append(dt,dd)}
  }catch(e){if(session)stopSession();token='';el('connection').textContent='Нет обновления: данные устарели или сеть недоступна';}
  controls();setTimeout(update,session?200:1000);
}
async function discrete(action,prompt){
  if(commandBusy||el(action).disabled||(prompt&&!confirm(prompt)))return;
  commandBusy=true;controls();
  try{
    if(action==='start'){generation=snapshot.calibGeneration;awaitingStart=Date.now();session=true;}
    await sendAction(action);
    el('calibMessage').textContent='Команда передана. Результат появится в состоянии калибровки.';
    if(action==='start')heartbeat();
    if(action==='save'||action==='reset')stopSession(false);
  }catch(e){el('calibMessage').textContent='Команда не подтверждена: '+e.message;stopSession();}
  finally{commandBusy=false;controls();}
}
el('start').onclick=()=>discrete('start','Кабина начнёт двигаться вверх до концевика. Начать?');
el('save').onclick=()=>discrete('save',null);
el('reset').onclick=()=>discrete('reset','Стереть сохранённую калибровку? Движение не запускается.');
el('stop').onclick=()=>{stopSession();el('calibMessage').textContent='Остановка запрошена. Проверьте состояние мотора.';};
el('down').onpointerdown=e=>{if(el('down').disabled||!session||heldDown)return;e.preventDefault();el('down').setPointerCapture(e.pointerId);heldDown=true;controls();sendAction('down').catch(()=>stopSession());};
function releaseDown(){if(!heldDown)return;heldDown=false;controls();sendAction('pause').catch(()=>stopSession());}
el('down').onpointerup=releaseDown;el('down').onpointercancel=releaseDown;el('down').onlostpointercapture=releaseDown;
window.addEventListener('blur',()=>{if(session)stopSession();});
window.addEventListener('pagehide',()=>{if(session)stopSession();});
document.addEventListener('visibilitychange',()=>{if(document.hidden&&session)stopSession();});
el('network').onclick=async()=>{if(!token||el('network').disabled||!confirm('Перейти в режим настройки сети?'))return;try{const r=await fetch('/api/network',{method:'POST',headers:{'X-Lift-Token':token}});el('message').textContent=await r.text();if(r.ok)el('network').disabled=true}catch(e){el('message').textContent='Проверьте наличие точки Lift-Setup'}};
controls();update();
</script></html>)HTML";
