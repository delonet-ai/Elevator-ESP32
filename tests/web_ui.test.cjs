// Exercise the actual embedded page script without a browser or hardware.
const {test} = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../src/lift/web_config.cpp'), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];

async function load(fetch) {
  const nodes = {};
  function node() { return {textContent:'', children:[], replaceChildren(){this.children=[];}, append(...v){this.children.push(...v);}}; }
  const context = vm.createContext({fetch, AbortSignal, confirm:()=>true, setTimeout:()=>{},
    document:{getElementById:id=>nodes[id]??(nodes[id]=node()), createElement:node}});
  vm.runInContext(script, context);
  await new Promise(resolve=>setImmediate(resolve));
  return nodes;
}

test('renders real status and sends authenticated network-change token', async()=>{
  const requests=[];
  const nodes=await load(async(url,options)=>{
    requests.push({url,options});
    return {ok:true, json:async()=>({state:1,floor:0,target:0,position:123,known:false,error:0,top:false,speed:42,peer:true,channel:11,uptime:5000,ip:'192.168.1.25',token:'test-token'}), text:async()=> 'Connect to Lift-Setup'};
  });
  assert.match(nodes.connection.textContent,/192.168.1.25/);
  assert.equal(nodes.values.children[1].textContent,'Нужна калибровка');
  assert.equal(nodes.values.children[17].textContent,11);
  await nodes.network.onclick();
  assert.equal(requests[1].options.method,'POST');
  assert.equal(requests[1].options.headers['X-Lift-Token'],'test-token');
  assert.equal(nodes.message.textContent,'Connect to Lift-Setup');
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
