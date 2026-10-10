#pragma once
#include <Arduino.h>

const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="ru"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Лифт · Настройки</title>
<style>
body{font:16px system-ui;background:#101826;color:#edf3ff;margin:0;padding:24px}main{max-width:760px;margin:auto}h1{margin-bottom:8px}.muted{color:#aabbd4}section{background:#1c293c;border-radius:16px;padding:24px;margin:20px 0}dl{display:grid;grid-template-columns:1fr 1fr;gap:14px}dd{margin:0;text-align:right}button:disabled{opacity:.45;cursor:default}button{background:#73d5b6;color:#10251f;border:0;border-radius:8px;padding:14px;font:inherit;cursor:pointer}.buttons{display:flex;flex-wrap:wrap;gap:10px}.stop{background:#ff8585}.secondary{background:#b2c5df}#down{touch-action:none;user-select:none}a{color:#73d5b6}
label{display:block;margin:12px 0}input{display:block;width:100%;box-sizing:border-box;padding:10px;margin-top:4px;font:inherit;border-radius:6px;border:1px solid #789;background:#101826;color:#edf3ff}
input[hidden]{display:none}
.controlbar{position:sticky;top:0;z-index:10;background:#101826;padding:10px 0;border-bottom:1px solid #43536b;display:flex;align-items:center;flex-wrap:wrap;gap:10px}.controlbar p{flex:1;margin:0;min-width:150px}nav{display:flex;flex-wrap:wrap;gap:16px;padding:16px 0}section{scroll-margin-top:130px}#eventsList{max-height:360px;overflow:auto;padding-left:24px}#eventsList li{margin:10px 0}dd{overflow-wrap:anywhere}@media(max-width:480px){body{padding:12px}section{padding:16px}dl{gap:10px;font-size:14px}}

</style>
<main><p class="muted">ELEVATOR ESP32</p><h1>Управление лифтом</h1><div class="controlbar"><p id="connection" role="status">Подключение…</p><button id="stop" class="stop" disabled>СТОП</button></div>
<nav aria-label="Разделы"><a href="#statusSection">Состояние</a><a href="#calibSection">Калибровка</a><a href="#motionSection">Скорости</a><a href="#networkSection">Wi-Fi</a><a href="#eventsSection">Журнал</a></nav>
<section id="statusSection"><h2>Состояние</h2><dl id="values"></dl><p id="errorHelp" role="status"></p>
<div class="buttons"><button id="home" disabled>Восстановить позицию по верху</button><button id="clear" class="secondary" disabled>Сбросить ошибку</button></div>
<p class="muted">Восстановление позиции поднимает кабину до концевика без стирания калибровки. Сброс ошибки сам по себе не запускает движение.</p></section>
<section id="calibSection"><h2>Калибровка</h2><p>1. Найдите верхний концевик. 2. Удерживайте спуск до нижней точки. 3. Отпустите кнопку, дождитесь остановки и сохраните низ.</p>
<p id="calibState">Ожидание данных</p><div class="buttons">
<button id="start" disabled>Найти верх</button><button id="down" disabled>Удерживать: вниз</button>
<button id="save" disabled>Сохранить низ</button>
<button id="reset" class="secondary" disabled>Сбросить калибровку</button></div>
<p id="calibMessage"></p><p class="muted">При уходе со страницы или потере связи веб-калибровка останавливается. Веб-СТОП не заменяет физическую аварийную кнопку. Удерживающий момент мотора сохраняется.</p></section>
<section id="motionSection"><h2>Движение</h2><p>Скорости в шагах/с, ускорение в шагах/с². Регулятор на базе выбирает скорость поездки между минимумом и максимумом. Ручной ход и калибровка используют отдельные скорости.</p><p class="muted">Пределы крутилки: 50–10 000 шагов/с. Исходный диапазон — 200–2000. Допустимая для механики скорость определяется при проверке.</p>
<label>Крутилка: скорость в минимальном положении<input id="minimum" type="number" min="50" max="10000" step="1" disabled></label>
<label>Крутилка: скорость в максимальном положении<input id="maximum" type="number" min="50" max="10000" step="1" disabled></label>
<label>Ручной ход<input id="manual" type="number" min="200" max="2000" step="1" disabled></label>
<label>Поиск верхнего концевика<input id="homing" type="number" min="200" max="2000" step="1" disabled></label>
<label>Спуск при калибровке<input id="calibDown" type="number" min="200" max="2000" step="1" disabled></label>
<label>Ускорение<input id="acceleration" type="number" min="100" max="1800" step="1" disabled></label>
<div class="buttons"><button id="motionSave" disabled>Сохранить параметры</button><button id="motionReload" class="secondary" disabled>Загрузить с базы</button><button id="motionDefaults" class="secondary" disabled>Подставить исходные</button></div>
<h3>Резервная копия</h3><div class="buttons"><button id="motionExport" class="secondary" disabled>Скачать параметры базы</button><button id="motionImport" class="secondary" disabled>Загрузить из файла</button></div>
<input id="motionFile" type="file" accept=".json,application/json" hidden>
<p class="muted">Файл содержит только скорости и ускорение. Загрузка заполняет поля для проверки; примените их кнопкой «Сохранить параметры».</p>
<p id="motionMessage"></p><p id="motionStorage" class="muted"></p><p class="muted">Сохранение доступно только после остановки и вне калибровки. Подстановка исходных значений требует сохранения. Настройки не запускают мотор.</p></section>
<section id="networkSection"><h2>Подключение к Wi-Fi</h2><p>Смена сети доступна после остановки, вне калибровки.</p>
<button id="network" disabled>Настроить другую сеть</button><p id="message"></p></section>
<section id="eventsSection"><h2>Журнал событий</h2><p class="muted">Последние 32 события с момента включения. Время — от запуска базы, позиция — в шагах. После перезапуска журнал очищается.</p>
<p id="eventsMessage">Ожидание журнала…</p><button id="eventsExport" class="secondary" disabled>Скачать журнал</button><ol id="eventsList"></ol></section></main>
<script>
const states=['Запуск','Нужна калибровка','Калибровка вверх','Калибровка вниз','Ожидание','Поездка','Ручное движение','Ошибка','Нужно найти верх','Поиск верха'];
const results=['','Поиск верхней точки запущен','Спуск приостановлен','Калибровка сохранена','Калибровка сброшена','Остановлено: потеря связи с браузером','Команда отклонена: проверьте состояние','Остановлено командой STOP','Ошибка контроллера — сессия прервана','Позиция восстановлена по верхнему концевику','Ошибка сброшена'];
const owner=crypto.getRandomValues(new Uint32Array(1))[0]||1;
let token='',snapshot=null,sequence=0,session=false,generation=0,heldDown=false,awaitingStart=0,commandBusy=false,heartbeatTimer=null;
const el=id=>document.getElementById(id);
let eventSnapshot=null;
const errorNames=['Нет ошибки','Таймаут движения','Верхний концевик','Таймаут калибровки','Недостаточный ход','Программный предел','Позиция не определена','Ошибка памяти'];
function eventText(e){
  const value=e.value;
  switch(e.kind){
    case 0:return 'Запуск базы';
    case 1:return 'Состояние: '+(states[value]??value);
    case 2:return 'Ошибка '+value+': '+(errorNames[value]??'Неизвестный код');
    case 3:return 'Команда STOP';
    case 4:return value?'Мотор движется':'Мотор остановлен';
    case 5:return value?'Верхний концевик нажат':'Верхний концевик свободен';
    case 6:return 'Веб-калибровка: '+(results[value]??value);
    case 8:return value?'Связь с пультом восстановлена':'Нет свежих пакетов от пульта';
    case 7:return 'Параметры движения: '+({2:'сохранены',3:'запрос отклонён',4:'ошибка записи'}[value]??value);
    default:return 'Событие '+e.kind+': '+value;
  }
}
async function updateEvents(){
  try{
    const r=await fetch('/api/events',{cache:'no-store',signal:AbortSignal.timeout(1200)});
    if(!r.ok)throw Error();const s=await r.json();
    if(typeof s.boot!=='string'||!Number.isInteger(s.age)||s.age<0||s.age>1000||
      !Number.isInteger(s.overwritten)||s.overwritten<0||!Array.isArray(s.events)||s.events.length>32||
      s.events.some(e=>!e||!['id','uptime','kind','value','position'].every(k=>Number.isInteger(e[k]))))throw Error();
    const restarted=eventSnapshot&&eventSnapshot.boot!==s.boot;
    eventSnapshot={boot:s.boot,overwritten:s.overwritten,events:s.events.map(e=>({id:e.id,uptime:e.uptime,kind:e.kind,value:e.value,position:e.position}))};
    const list=el('eventsList');list.replaceChildren();
    for(const e of [...s.events].reverse()){
      const item=document.createElement('li');
      item.textContent=(e.uptime/1000).toFixed(1)+' с · '+eventText(e)+' · позиция '+e.position;
      list.append(item);
    }
    el('eventsMessage').textContent=(restarted?'База перезапущена. ':'')+
      (s.events.length?'Журнал обновлён.':'Событий пока нет.')+(s.overwritten?' Вытеснено старых событий: '+s.overwritten+'.':'');
    el('eventsExport').disabled=false;
  }catch(e){el('eventsMessage').textContent='Журнал не обновлён: показанные события могут быть устаревшими.';el('eventsExport').disabled=true;}
  setTimeout(updateEvents,2000);
}
el('eventsExport').onclick=()=>{
  if(el('eventsExport').disabled||!eventSnapshot)return;
  const link=document.createElement('a');
  link.href='data:application/json;charset=utf-8,'+encodeURIComponent(JSON.stringify({format:'elevator-esp32-events',version:1,...eventSnapshot},null,2)+'\n');
  link.download='lift-events.json';document.body.append(link);link.click();link.remove();
};
const motionFields={minimum:'minimum',maximum:'maximum',manual:'manual',homing:'homing',down:'calibDown',acceleration:'acceleration'};
let motionDirty=false,motionFormRevision=0,motionPending=null,motionBootToken='',motionImporting=false;
function validMotion(values){
  return !!values&&typeof values==='object'&&!Array.isArray(values)&&
    Object.keys(values).length===Object.keys(motionFields).length&&
    Object.keys(motionFields).every(k=>Object.hasOwn(values,k)&&Number.isInteger(values[k])&&
      values[k]>=(k==='acceleration'?100:k==='minimum'||k==='maximum'?50:200)&&values[k]<=(k==='acceleration'?1800:k==='minimum'||k==='maximum'?10000:2000))&&values.minimum<=values.maximum;
}
function motionAvailable(){return !!token&&!!snapshot?.motion&&snapshot.stationary&&!snapshot.calibOwner&&!session&&!motionPending&&snapshot.networkResult!==1;}
el('motionExport').onclick=()=>{
  if(el('motionExport').disabled)return;
  const motion=Object.fromEntries(Object.keys(motionFields).map(k=>[k,snapshot.motion[k]]));
  if(!validMotion(motion)){el('motionMessage').textContent='Не удалось проверить параметры базы.';return;}
  const file={format:'elevator-esp32-motion',version:1,motion};
  const link=document.createElement('a');
  link.href='data:application/json;charset=utf-8,'+encodeURIComponent(JSON.stringify(file,null,2)+'\n');
  link.download='lift-motion.json';document.body.append(link);link.click();link.remove();
  el('motionMessage').textContent='Файл текущих параметров базы подготовлен. Несохранённые правки в него не входят.';
};
el('motionImport').onclick=()=>{if(!el('motionImport').disabled)el('motionFile').click();};
el('motionFile').onchange=async()=>{
  const input=el('motionFile'),file=input.files?.[0];input.value='';
  if(!file||!motionAvailable()||motionImporting)return;
  const initialToken=token,initialRevision=snapshot.motionRevision;
  motionImporting=true;controls();
  try{
    if(file.size>4096)throw Error('Файл больше 4 КБ.');
    const text=await file.text();
    if(text.length>4096)throw Error('Файл больше 4 КБ.');
    let data;try{data=JSON.parse(text.replace(/^\uFEFF/,''));}catch(e){throw Error('Некорректный JSON.');}
    if(!data||data.format!=='elevator-esp32-motion'||data.version!==1||
        Object.keys(data).length!==3||!Object.hasOwn(data,'motion'))throw Error('Неизвестный формат или версия файла.');
    if(!validMotion(data.motion))throw Error('Недопустимые или неполные параметры движения.');
    if(!motionAvailable()||token!==initialToken||snapshot.motionRevision!==initialRevision)
      throw Error('Состояние базы изменилось. Повторите загрузку после обновления данных.');
    fillMotion(data.motion,initialRevision);motionDirty=true;
    el('motionMessage').textContent='Параметры из файла загружены в поля. Проверьте их и нажмите «Сохранить параметры».';
  }catch(e){el('motionMessage').textContent='Файл не загружен: '+e.message;}
  finally{motionImporting=false;controls();}
};
function fillMotion(values,revision){for(const [key,id] of Object.entries(motionFields))el(id).value=String(values[key]);motionFormRevision=revision;}
function syncMotion(s,requestAtStart,acknowledgedAtStart){
  if(!s.motion)return;
  if(motionBootToken&&motionBootToken!==s.token){
    motionPending=null;
    if(motionDirty){motionFormRevision=0;el('motionMessage').textContent='База перезапущена. Загрузите актуальные параметры перед сохранением.';}
  }
  motionBootToken=s.token;
  el('motionStorage').textContent=['Используются исходные значения: сохранённой записи ещё нет.','Параметры сохранены в памяти базы.','Запись не прочитана или повреждена: используются исходные значения.'][s.motionStorage]||'';
  if(motionPending&&motionPending===requestAtStart&&acknowledgedAtStart){
    if(s.motionRevision!==motionPending.revision){
      const matches=Object.keys(motionFields).every(k=>s.motion[k]===motionPending.values[k]);
      el('motionMessage').textContent=matches?'Параметры сохранены и применены.':'Настройки изменены другой вкладкой. Загрузите значения с базы.';
      motionPending=null;if(matches)motionDirty=false;
    }else if(s.motionResult===3||s.motionResult===4){
      el('motionMessage').textContent=s.motionResult===4?'Ошибка записи в память. Прежние параметры сохранены.':'Запрос отклонён: лифт занят или данные устарели.';motionPending=null;
    }else if(Date.now()-motionPending.started>5000){el('motionMessage').textContent='Нет подтверждения. Загрузите параметры с базы перед повтором.';motionPending=null;}
  }
  if(!motionDirty&&!motionPending)fillMotion(s.motion,s.motionRevision);
}
for(const id of Object.values(motionFields))el(id).oninput=()=>{motionDirty=true;};
el('motionReload').onclick=()=>{if(el('motionReload').disabled)return;fillMotion(snapshot.motion,snapshot.motionRevision);motionDirty=false;el('motionMessage').textContent='Загружены текущие параметры базы.';};
el('motionDefaults').onclick=()=>{if(el('motionDefaults').disabled)return;fillMotion({minimum:200,maximum:2000,manual:400,homing:400,down:1200,acceleration:1800},motionFormRevision);motionDirty=true;el('motionMessage').textContent='Исходные значения подставлены. Для применения сохраните.';};
el('motionSave').onclick=async()=>{
  if(el('motionSave').disabled)return;
  const values={};for(const [key,id] of Object.entries(motionFields)){const text=el(id).value;values[key]=/^\d+$/.test(text)?Number(text):NaN;}
  if(!validMotion(values)){el('motionMessage').textContent='Проверьте диапазоны: крутилка 50–10000, ручной ход и калибровка 200–2000, ускорение 100–1800, минимум ≤ максимум.';return;}
  motionPending={values,revision:motionFormRevision,started:Date.now(),acknowledged:false};
  el('motionMessage').textContent='Отправка параметров…';controls();
  try{
    const r=await fetch('/api/motion',{method:'POST',headers:{'X-Lift-Token':token},body:new URLSearchParams({...values,revision:motionFormRevision}),signal:AbortSignal.timeout(2000)});
    if(!r.ok)throw Error(await r.text());
    if(motionPending){motionPending.acknowledged=true;el('motionMessage').textContent='Запрос передан. Ожидание подтверждения базы…';}
  }catch(e){motionPending=null;el('motionMessage').textContent='Сохранение не подтверждено: '+e.message;}
  controls();
};
function controls(){
  const live=!!token&&!!snapshot, mine=live&&snapshot.calibOwner===owner;
  el('start').disabled=!live||!snapshot.calibCanStart||session||commandBusy;
  el('down').disabled=!mine||!session||snapshot.state!==3;
  el('save').disabled=!mine||!session||!snapshot.calibCanSave||heldDown||commandBusy;
  el('reset').disabled=!live||!snapshot.calibCanStart||session||commandBusy;
  el('stop').disabled=!token;
  el('home').disabled=!live||!snapshot.stationary||snapshot.state!==8||!!snapshot.calibOwner||session||commandBusy;
  el('clear').disabled=!live||!snapshot.stationary||snapshot.state!==7||!!snapshot.calibOwner||session||commandBusy;
  const editable=motionAvailable()&&!motionImporting;
  for(const id of Object.values(motionFields))el(id).disabled=!editable;
  el('motionSave').disabled=!editable||!motionFormRevision;el('motionDefaults').disabled=!editable;
  el('motionReload').disabled=!live||!snapshot.motion||!!motionPending||motionImporting;
  el('motionImport').disabled=!editable;
  el('motionExport').disabled=!live||!snapshot.motion||!!motionPending||motionImporting;
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
  const requestAtStart=motionPending,acknowledgedAtStart=!!motionPending?.acknowledged;
  try{
    const r=await fetch('/api/status',{cache:'no-store',signal:AbortSignal.timeout(1200)});
    if(!r.ok)throw Error();const s=await r.json();if(s.age>1000)throw Error('stale');
    token=s.token;snapshot=s;syncMotion(s,requestAtStart,acknowledgedAtStart);
    if(session){
      if(s.calibGeneration!==generation||(s.calibOwner&&s.calibOwner!==owner))stopSession(false);
      else if(s.calibOwner===owner)awaitingStart=0;
      else if(!awaitingStart||Date.now()-awaitingStart>1000)stopSession();
    }
    if(s.networkResult===2)el('message').textContent='Смена сети отменена: лифт начал движение или запрос устарел';
    el('connection').textContent='База доступна · '+s.ip;
    el('errorHelp').textContent=s.error?'Устраните причину ошибки перед сбросом. При неизвестной позиции потребуется восстановление по верхнему концевику.':s.state===8?'Калибровка сохранена, но после запуска положение кабины нужно восстановить.':'';
    const phase=s.calibOwner&&s.state===2?'Подъём до верхнего концевика':s.calibOwner&&s.state===3?(s.running?'Спуск. Отпустите кнопку для остановки':'Верх найден. Удерживайте вниз или сохраните нижнюю точку'):(results[s.calibResult]||states[s.state]);
    el('calibState').textContent=(s.calibOwner&&s.calibOwner!==owner?'Управляет другая вкладка. ':'')+phase;
    const rows=[['Состояние',states[s.state]||s.state],['Этаж / цель',s.floor+' / '+s.target],['Позиция, шагов',s.position],['Позиция известна',s.known?'Да':'Нет'],['Ошибка',s.error?(errorNames[s.error]??'Неизвестная ошибка')+' (код '+s.error+')':'Нет'],['Верхний концевик',s.top?'Нажат':'Свободен'],['Регулятор скорости',s.speed+'%'],['Пульт зарегистрирован',s.peer?'Да':'Нет'],['Радиоканал',s.channel],['Время работы, с',Math.floor(s.uptime/1000)],['Мотор',s.running?'Движется':'Остановлен'],['Ход, шагов',s.travel],['Свободная память, КБ',Math.round(s.freeHeap/1024)],['Сигнал Wi-Fi',s.rssi+' dBm'],['Пакеты от пульта',s.remoteOnline?'Приходят':s.remoteSeen?'Нет свежих пакетов':'Ещё не получены'],['Последний пакет пульта',s.remoteSeen?(s.remoteAgeMs/1000).toFixed(1)+' с назад':'—']];
    const box=el('values');box.replaceChildren();for(const [k,v]of rows){const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=k;dd.textContent=v;box.append(dt,dd)}
  }catch(e){if(session)stopSession();token='';el('connection').textContent='Нет обновления: данные устарели или сеть недоступна';}
  controls();setTimeout(update,session?200:1000);
}
async function discrete(action,prompt){
  if(commandBusy||el(action).disabled||(prompt&&!confirm(prompt)))return;
  commandBusy=true;controls();
  try{
    if(action==='start'||action==='home'){generation=snapshot.calibGeneration;awaitingStart=Date.now();session=true;}
    await sendAction(action);
    el('calibMessage').textContent='Команда передана. Результат появится в состоянии калибровки.';
    if(action==='start'||action==='home')heartbeat();
    if(action==='save'||action==='reset')stopSession(false);
  }catch(e){el('calibMessage').textContent='Команда не подтверждена: '+e.message;stopSession();}
  finally{commandBusy=false;controls();}
}
el('home').onclick=()=>discrete('home','Кабина начнёт двигаться вверх до концевика. Восстановить позицию?');
el('clear').onclick=()=>discrete('clear',null);
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
controls();update();updateEvents();
</script></html>)HTML";
