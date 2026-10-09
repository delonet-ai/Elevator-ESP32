// Exercise the actual embedded page script without a browser or hardware.
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../src/lift/web_page.h'), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];

async function load(fetch) {
  const nodes = {};
  const events = {};
  function node() { return {textContent:'', children:[], setPointerCapture(){}, replaceChildren(){this.children=[];}, append(...v){this.children.push(...v);}}; }
  const context = vm.createContext({fetch, AbortSignal, URLSearchParams, Uint32Array,
    crypto:{getRandomValues:a=>{a[0]=42;return a;}}, confirm:()=>true, setTimeout:()=>{}, clearTimeout:()=>{},
    window:{addEventListener:(name,fn)=>events[name]=fn},
    document:{addEventListener:(name,fn)=>events[name]=fn, hidden:false,getElementById:id=>nodes[id]??(nodes[id]=node()), createElement:node}});
  vm.runInContext(script, context);
  await new Promise(resolve=>setImmediate(resolve));
  nodes.refresh = () => vm.runInContext('update()', context);
  nodes.event = name => events[name]();
  return nodes;
}

const defaultMotion={minimum:200,maximum:2000,manual:400,homing:400,down:1200,acceleration:1800};
function motionStatus(extra={}) { return {state:4,age:0,token:'boot-1',stationary:true,calibOwner:0,motion:{...defaultMotion},motionRevision:1,motionStorage:0,motionResult:0,...extra}; }

test('old status received while settings POST is pending cannot reject the new request',async()=>{
  let status=motionStatus({motionResult:4}),reply;
  const nodes=await load(async(url,options)=>{
    if(options?.method==='POST')return new Promise(resolve=>reply=resolve);
    return {ok:true,json:async()=>status};
  });
  nodes.manual.value='600';nodes.manual.oninput();const sending=nodes.motionSave.onclick();
  await nodes.refresh();assert.equal(nodes.motionSave.disabled,true);
  assert.doesNotMatch(nodes.motionMessage.textContent,/Ошибка записи/);
  reply({ok:true});await sending;
  status={...status,motionRevision:2,motionResult:2,motion:{...defaultMotion,manual:600}};
  await nodes.refresh();assert.match(nodes.motionMessage.textContent,/сохранены и применены/);
});

test('motion edits survive polling and wait for persisted revision before success',async()=>{
  let status=motionStatus();const posts=[];
  const nodes=await load(async(url,options)=>{
    if(options?.method==='POST')posts.push({url,options});
    return {ok:true,json:async()=>status,text:async()=> 'Queued'};
  });
  assert.equal(nodes.maximum.value,'2000');
  nodes.maximum.value='1400';nodes.maximum.oninput();
  await nodes.refresh();assert.equal(nodes.maximum.value,'1400');
  await nodes.motionSave.onclick();
  assert.equal(posts[0].url,'/api/motion');
  assert.equal(posts[0].options.headers['X-Lift-Token'],'boot-1');
  assert.equal(posts[0].options.body.get('maximum'),'1400');
  assert.equal(posts[0].options.body.get('revision'),'1');
  assert.match(nodes.motionMessage.textContent,/Ожидание/);
  status={...status,motionResult:2};await nodes.refresh();
  assert.equal(nodes.motionSave.disabled,true); // Result without new snapshot is insufficient.
  status={...status,motionRevision:2,motion:{...defaultMotion,maximum:1400}};
  await nodes.refresh();assert.match(nodes.motionMessage.textContent,/сохранены и применены/);
  assert.equal(nodes.motionSave.disabled,false);
});

test('motion storage failure preserves edits; reload discards them explicitly',async()=>{
  let status=motionStatus();
  const nodes=await load(async()=>({ok:true,json:async()=>status,text:async()=> 'Queued'}));
  nodes.manual.value='600';nodes.manual.oninput();await nodes.motionSave.onclick();
  status={...status,motionResult:4};await nodes.refresh();
  assert.match(nodes.motionMessage.textContent,/Ошибка записи/);assert.equal(nodes.manual.value,'600');
  nodes.motionReload.onclick();assert.equal(nodes.manual.value,'400');
});

test('motion form rejects invalid input and disables changes during calibration or movement',async()=>{
  let status=motionStatus();let posts=0;
  const nodes=await load(async(url,options)=>{if(options?.method==='POST')posts++;return {ok:true,json:async()=>status};});
  nodes.minimum.value='2001';nodes.minimum.oninput();await nodes.motionSave.onclick();
  assert.equal(posts,0);assert.match(nodes.motionMessage.textContent,/диапазоны/);
  status={...status,stationary:false,state:3};await nodes.refresh();
  assert.equal(nodes.motionSave.disabled,true);assert.equal(nodes.manual.disabled,true);
  status={...status,stationary:true,state:4,calibOwner:42};await nodes.refresh();assert.equal(nodes.motionSave.disabled,true);
});

test('another tab cannot silently replace a dirty form; reboot requires reload',async()=>{
  let status=motionStatus();const posts=[];
  const nodes=await load(async(url,options)=>{if(options?.method==='POST')posts.push(options);return {ok:true,json:async()=>status};});
  nodes.manual.value='600';nodes.manual.oninput();
  status={...status,motionRevision:2,motion:{...defaultMotion,manual:800}};
  await nodes.refresh();assert.equal(nodes.manual.value,'600');
  await nodes.motionSave.onclick();assert.equal(posts[0].body.get('revision'),'1');
  await nodes.refresh();assert.match(nodes.motionMessage.textContent,/другой вкладкой/);
  status={...status,token:'boot-2',motionRevision:1};await nodes.refresh();
  assert.equal(nodes.motionSave.disabled,true);assert.match(nodes.motionMessage.textContent,/перезапущена/);
  nodes.motionReload.onclick();await nodes.refresh();
  assert.equal(nodes.motionSave.disabled,false);assert.equal(nodes.manual.value,'800');
});

test('motion defaults only populate the form and do not send a command',async()=>{
  let posts=0;const nodes=await load(async(url,options)=>{if(options?.method==='POST')posts++;return {ok:true,json:async()=>motionStatus({motion:{...defaultMotion,manual:600}})};});
  nodes.motionDefaults.onclick();assert.equal(nodes.manual.value,'400');assert.equal(posts,0);
  await nodes.refresh();assert.equal(nodes.manual.value,'400');
});

test('renders real status and sends authenticated network-change token', async()=>{
  const requests=[];
  const nodes=await load(async(url,options)=>{
    requests.push({url,options});
    return {ok:true, json:async()=>({state:1,floor:0,target:0,position:123,known:false,error:0,top:false,speed:42,peer:true,channel:11,uptime:5000,ip:'192.168.1.25',token:'test-token',stationary:true,age:50,networkResult:0,running:false,travel:2000,freeHeap:65536,rssi:-50}), text:async()=> 'Connect to Lift-Setup'};
  });
  assert.match(nodes.connection.textContent,/192.168.1.25/);
  assert.equal(nodes.values.children[1].textContent,'Нужна калибровка');
  assert.equal(nodes.values.children[17].textContent,11);
  await nodes.network.onclick();
  assert.equal(requests[1].options.method,'POST');
  assert.equal(requests[1].options.headers['X-Lift-Token'],'test-token');
  assert.equal(nodes.message.textContent,'Connect to Lift-Setup');
  assert.equal(nodes.network.disabled,true);
});

test('web calibration sends holds, pauses on release and stops on leaving the tab',async()=>{
  const requests=[];
  let status={state:1,age:0,stationary:true,calibOwner:0,calibGeneration:8,calibCanStart:true,token:'secret'};
  const nodes=await load(async(url,options)=>{
    requests.push({url,options});
    return {ok:true,json:async()=>status,text:async()=> 'Queued'};
  });
  await nodes.start.onclick();
  const commands=()=>requests.filter(x=>x.url==='/api/calibration').map(x=>Object.fromEntries(x.options.body));
  assert.equal(commands()[0].action,'start');
  assert.equal(commands()[0].owner,'42');
  assert.equal(commands()[0].generation,'8');
  status={...status,state:3,stationary:false,calibOwner:42,calibCanStart:false,calibCanSave:true};
  await nodes.refresh();
  assert.equal(nodes.down.disabled,false);
  nodes.down.onpointerdown({pointerId:1,preventDefault(){}});
  assert.equal(commands().at(-1).action,'down');
  assert.equal(nodes.save.disabled,true);
  nodes.down.onpointerup();
  assert.equal(commands().at(-1).action,'pause');
  const actions=commands();
  assert.ok(Number(actions.at(-1).sequence)>Number(actions.at(-2).sequence));
  nodes.event('blur');
  assert.equal(requests.at(-1).url,'/api/stop');
  assert.equal(nodes.down.disabled,true);
});

test('expired generation never restarts an active browser session automatically',async()=>{
  const requests=[];
  let status={state:1,age:0,stationary:true,calibOwner:0,calibGeneration:8,calibCanStart:true,token:'secret'};
  const nodes=await load(async(url,options)=>{requests.push({url,options});return {ok:true,json:async()=>status};});
  await nodes.start.onclick();
  status={...status,calibGeneration:9,calibResult:5};
  await nodes.refresh();
  assert.equal(nodes.down.disabled,true);
  assert.equal(requests.filter(x=>x.url==='/api/calibration'&&x.options.body.get('action')==='start').length,1);
});

test('another tab cannot operate the active calibration',async()=>{
  const nodes=await load(async()=>({ok:true,json:async()=>({state:3,age:0,token:'secret',calibOwner:99,calibGeneration:8,calibCanSave:true})}));
  assert.equal(nodes.down.disabled,true);
  assert.equal(nodes.save.disabled,true);
  assert.equal(nodes.stop.disabled,false);
  assert.match(nodes.calibState.textContent,/другая вкладка/);
});

test('network failure reports stale status and does not enable configuration without token',async()=>{
  let requests=0;
  const nodes=await load(async()=>{requests++;throw Error('offline');});
  assert.match(nodes.connection.textContent,/Нет обновления/);
  await nodes.network.onclick();
  assert.equal(requests,1);
});

test('an authentication failure is not presented as fresh data',async()=>{
  const nodes=await load(async()=>({ok:false}));
  assert.match(nodes.connection.textContent,/Нет обновления/);
});

test('moving status stays visible while network change is disabled',async()=>{
  let calls=0;
  const nodes=await load(async()=>{calls++;return {ok:true,json:async()=>({state:5,ip:'192.168.1.25',age:50,stationary:false,token:'secret',running:true})};});
  assert.equal(nodes.values.children[1].textContent,'Поездка');
  assert.equal(nodes.network.disabled,true);
  await nodes.network.onclick();
  assert.equal(calls,1);
});

test('old controller snapshot is not treated as live even over a working HTTP connection',async()=>{
  const nodes=await load(async()=>({ok:true,json:async()=>({age:2000,stationary:true,token:'secret'})}));
  assert.match(nodes.connection.textContent,/Нет обновления/);
  assert.equal(nodes.network.disabled,true);
});

test('loss after a successful update clears permission to configure',async()=>{
  let calls=0;
  const nodes=await load(async()=>{
    if (++calls>1) throw Error('offline');
    return {ok:true,json:async()=>({state:4,age:0,stationary:true,token:'secret'})};
  });
  assert.equal(nodes.network.disabled,false);
  await nodes.refresh();
  await nodes.network.onclick();
  assert.equal(nodes.network.disabled,true);
  assert.equal(calls,2);
});

test('a request rejected by the control loop is reported instead of being deferred',async()=>{
  const nodes=await load(async()=>({ok:true,json:async()=>({state:5,age:0,stationary:false,networkResult:2,token:'secret'})}));
  assert.match(nodes.message.textContent,/Смена сети отменена/);
});
