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
  function node() { return {textContent:'', children:[], replaceChildren(){this.children=[];}, append(...v){this.children.push(...v);}}; }
  const context = vm.createContext({fetch, AbortSignal, confirm:()=>true, setTimeout:()=>{},
    document:{getElementById:id=>nodes[id]??(nodes[id]=node()), createElement:node}});
  vm.runInContext(script, context);
  await new Promise(resolve=>setImmediate(resolve));
  nodes.refresh = () => vm.runInContext('update()', context);
  return nodes;
}

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
