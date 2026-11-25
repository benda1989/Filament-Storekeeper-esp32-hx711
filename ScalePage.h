#pragma once

constexpr char PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>耗材秤</title>
  <style>
    :root { color-scheme:light; font-family:system-ui,sans-serif; --ink:#172b28; --muted:#59706b; --line:#cbd7d2; --green:#176b55; --red:#a34731; --paper:#f5f8f6; --white:#fff; --radius:4px; --space:18px; --motion:150ms; }
    * { box-sizing:border-box; }
    html { scrollbar-gutter:stable; }
    body { margin:0; background:var(--paper); color:var(--ink); }
    main { max-width:860px; margin:auto; padding:22px 18px 48px; }
    header { display:flex; justify-content:space-between; align-items:center; gap:12px; padding-bottom:16px; border-bottom:1px solid var(--line); }
    .nav-button { display:inline-flex; align-items:center; justify-content:center; min-height:42px; padding:0 14px; border:1px solid var(--line); border-radius:var(--radius); background:var(--white); color:var(--ink); font-size:14px; text-decoration:none; white-space:nowrap; }
    .nav-button:hover { background:#e9efec; }
    h1 { margin:0; font-size:22px; }
    h2 { font-size:17px; margin:0 0 14px; }
    .header-actions { display:flex; align-items:center; gap:12px; flex-shrink:0; }
    .online { color:var(--green); font-size:13px; }
    .modes { display:inline-flex; margin:22px 0; border:1px solid var(--line); border-radius:var(--radius); overflow:hidden; }
    .modes button { border:0; border-radius:0; background:var(--white); color:var(--ink); min-width:112px; }
    .modes button[aria-pressed="true"] { background:var(--green); color:white; }
    button,input,select { font:inherit; min-height:42px; border-radius:var(--radius); }
    button { font-size:14px; cursor:pointer; padding:0 14px; background:var(--green); color:white; border:1px solid var(--green); transition:background var(--motion); }
    button:hover:not(:disabled) { background:#10523f; }
    button.secondary { background:var(--white); color:var(--ink); border-color:var(--line); }
    button.secondary:hover:not(:disabled) { background:#e9efec; }
    button:disabled { opacity:.5; cursor:default; }
    input,select { font-size:16px; min-width:0; max-width:100%; padding:0 10px; border:1px solid #9cb2a9; background:white; color:var(--ink); }
    button:focus-visible,a:focus-visible,input:focus-visible,select:focus-visible { outline:2px solid var(--green); outline-offset:3px; }
    input[type="color"] { width:74px; padding:4px; }
    label { display:grid; gap:5px; color:var(--muted); font-size:13px; }
    .readout { display:grid; grid-template-columns:8px 1fr; min-height:172px; background:#193c33; color:white; border-radius:var(--radius); overflow:hidden; }
    .rail { background:repeating-linear-gradient(to bottom,#b9d8cb 0 2px,#193c33 2px 13px); }
    .readout-body { padding:26px 24px; }
    .readout .small { opacity:.76; font-size:14px; }
    .value { margin-top:12px; font-variant-numeric:tabular-nums; font-size:64px; line-height:1.15; overflow-wrap:anywhere; }
    .unit { font-size:22px; opacity:.75; margin-left:7px; }
    .gross-line { margin-top:16px; padding-top:12px; border-top:1px solid #66867c; display:flex; justify-content:space-between; gap:12px; font-size:14px; font-variant-numeric:tabular-nums; }
    .gross-line strong { color:white; font-size:18px; font-weight:600; }
    .readout-empty { display:flex; align-items:center; padding:26px 24px; font-size:32px; font-weight:600; }
    #status { font-size:13px; line-height:1.6; min-height:26px; margin:15px 0 0; color:var(--muted); }
    section { border-top:1px solid var(--line); padding:22px 0; }
    .section-heading { display:flex; align-items:center; justify-content:space-between; gap:12px; margin-bottom:14px; }
    .section-heading h2 { margin:0; }
    .section-heading button { flex:none; }
    .row { display:flex; flex-wrap:wrap; align-items:end; gap:10px; }
    .note,.meta { color:var(--muted); font-size:13px; line-height:1.5; }
    .spool-list { display:grid; }
    .spool-row { display:flex; align-items:center; gap:12px; border-bottom:1px solid var(--line); padding:12px 0; min-width:0; }
    .swatch { flex:none; width:26px; height:26px; border:1px solid #9cadab; border-radius:3px; }
    .spool-text { flex:1; min-width:0; }
    .spool-text strong { display:block; font-size:14px; }
    .spool-text small { color:var(--muted); font-size:12px; }
    .spool-actions { display:flex; gap:8px; margin-left:auto; }
    .spool-actions button { min-width:62px; }
    .edit-form { display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); gap:12px; margin-top:20px; }
    .edit-form .row { grid-column:1/-1; }
    .hidden { display:none !important; }
    .error { color:var(--red) !important; }
    .spool-text small.warning { color:#916014; }
    .scale-actions { display:flex; flex-wrap:wrap; gap:10px; margin-bottom:16px; }
    .weight-log { width:100%; table-layout:fixed; border-collapse:collapse; margin-top:14px; font-size:13px; }
    .weight-log th,.weight-log td { padding:12px 6px; text-align:left; vertical-align:top; border-bottom:1px solid var(--line); overflow-wrap:anywhere; }
    .weight-log th:first-child { width:32%; }
    .weight-log th:nth-child(2) { width:20%; }
    .weight-log time { color:var(--muted); font-size:12px; }
    .weight-log td:nth-child(2) { font-variant-numeric:tabular-nums; }
    .weight-log textarea { display:block; width:100%; min-height:42px; padding:8px; border:1px solid #9cb2a9; border-radius:var(--radius); font:inherit; color:var(--ink); background:var(--white); resize:vertical; }
    #weightLogMessage:empty { display:none; }
    @media (max-width:600px) { .value { font-size:48px; } .edit-form { grid-template-columns:1fr 1fr; } .edit-form label:last-of-type { grid-column:1/-1; } .spool-row { flex-wrap:wrap; } .spool-text { min-width:calc(100% - 44px); } .spool-actions { width:100%; display:grid; grid-template-columns:repeat(3,minmax(0,1fr)); margin-left:0; } .spool-actions button { min-width:0; width:100%; } }
  </style>
</head>
<body>
  <main>
    <header><h1>耗材秤</h1><div class="header-actions"><span class="online" id="online">连接中</span><a class="nav-button" href="/settings">设置</a></div></header>

    <div class="modes" role="group" aria-label="称重模式"><button id="spoolMode" aria-pressed="true">耗材模式</button><button id="scaleMode" aria-pressed="false">普通模式</button></div>
    <div class="readout"><div class="rail"></div><div class="readout-body" id="measurements"><div class="small" id="readingLabel">当前耗材余量</div><div class="value"><span id="weight">--</span><span class="unit">g</span></div><div id="grossLine" class="gross-line"><span>称重总量</span><strong><span id="gross">--</span> g</strong></div></div><div class="readout-empty hidden" id="noSpool">无耗材</div></div>
    <p id="status">正在读取...</p>
    <a id="calibrationLink" class="nav-button hidden" href="/settings">前往设置校准</a>

    <section id="scaleSection" class="hidden">
      <div class="scale-actions"><button id="ordinaryTare" type="button">去皮</button><button id="recordWeight" type="button" disabled>记录</button><button id="exportWeightLog" class="secondary" type="button" disabled>导出 CSV</button></div>
      <p id="weightLogMessage" class="note" role="status" aria-live="polite"></p>
      <table class="weight-log" aria-label="称重记录"><thead><tr><th scope="col">日期</th><th scope="col">重量 (g)</th><th scope="col">说明</th></tr></thead><tbody id="weightLogRows"></tbody></table>
      <p id="weightLogEmpty" class="note">暂无记录</p>
    </section>

    <section id="inventorySection">
      <div class="section-heading"><h2>耗材记录 <span id="count"></span></h2><button id="create" type="button">新建耗材</button></div>
      <div class="spool-list" id="spoolList"></div>
      <form id="editForm" class="edit-form hidden">
        <label>颜色<input id="editColor" type="color" value="#808080"></label>
        <label>材料<select id="editMaterial"><option value="0">PLA</option><option value="1">PETG</option><option value="2">ABS</option><option value="3">TPU</option><option value="4">未设置 / 其他</option></select></label>
        <label>空盘重量 (g)<input id="editTare" type="number" min="0" step="any" inputmode="decimal"></label>
        <div class="row"><button type="submit">保存记录</button><button id="cancelEdit" class="secondary" type="button">取消</button></div>
      </form>
    </section>

  </main>
  <script>
    const el = id => document.getElementById(id);
    const materials = ['PLA','PETG','ABS','TPU','未设置 / 其他'];
    let lastGoodReading = null, lastGoodMode = null;
    let state = null, records = [], displayMode = 'spool', modeReady = false, localTare = 0;
    let requestQueue = Promise.resolve(), pollTimer = null, readFailures = 0;
    let editId = null, busy = false, errorUntil = 0;
    let listLoaded = false, listTag = '', nextListAt = 0, inventoryError = '';
    const weightLogKey='hx711.ordinary-log.v1';
    let weightLog=[],weightLogReadable=true;
    const format = value => value == null ? '--' : Number(value).toFixed(1);
    const remaining = (record,total=record.total) => record.tare==null || total==null ? null : Math.max(0,total-record.tare);
    const logDate=date=>new Date(date).toLocaleString('zh-CN',{hour12:false});
    function displayReading() {
      return state?.state===98 && lastGoodMode===displayMode && lastGoodReading ? lastGoodReading : state;
    }
    function scaleWeight() {
      return modeReady && !readFailures && state && (state.state===0||state.state===1) && Number.isFinite(state.value) ? state.value-localTare : null;
    }
    function updateScaleControls() {
      const unavailable=busy||scaleWeight()==null;
      el('ordinaryTare').disabled=unavailable;
      el('recordWeight').disabled=unavailable||displayMode!=='scale';
      el('exportWeightLog').disabled=!weightLog.length;
      el('spoolMode').disabled=el('scaleMode').disabled=busy;
    }
    function logMessage(text,error=false) {
      el('weightLogMessage').textContent=text;el('weightLogMessage').classList.toggle('error',error);
    }
    function persistWeightLog(success) {
      try {
        if(!weightLogReadable)throw new Error('原记录未能读取');
        localStorage.setItem(weightLogKey,JSON.stringify(weightLog));
        logMessage(success);return true;
      } catch(error) {logMessage('未能保存到浏览器，当前记录仅临时保留，请导出 CSV 备份。',true);return false;}
    }
    function renderWeightLog() {
      const rows=el('weightLogRows');rows.replaceChildren();
      weightLog.forEach((record,index)=>{
        const row=document.createElement('tr'),date=document.createElement('td'),time=document.createElement('time'),weight=document.createElement('td'),note=document.createElement('td'),input=document.createElement('textarea');
        time.dateTime=record.date;time.textContent=logDate(record.date);date.append(time);
        weight.textContent=record.weight;input.rows=1;input.value=record.note;input.setAttribute('aria-label',`${logDate(record.date)} 的说明`);
        input.oninput=()=>{weightLog[index].note=input.value;persistWeightLog('说明已保存');};
        note.append(input);row.append(date,weight,note);rows.append(row);
      });
      el('weightLogEmpty').classList.toggle('hidden',!!weightLog.length);updateScaleControls();
    }
    function loadWeightLog() {
      try {
        const saved=JSON.parse(localStorage.getItem(weightLogKey)??'[]');
        if(!Array.isArray(saved)||saved.some(record=>!record||typeof record.date!=='string'||!Number.isFinite(Date.parse(record.date))||typeof record.weight!=='string'||!record.weight.trim()||!Number.isFinite(Number(record.weight))||typeof record.note!=='string'))throw new Error('记录格式异常');
        weightLog=saved;
      } catch(error) {weightLogReadable=false;logMessage('未能读取浏览器记录，新增记录将临时保留，请导出 CSV 备份。',true);}
      renderWeightLog();
    }
    function recordWeight() {
      const weight=scaleWeight();if(busy||displayMode!=='scale'||weight==null)return;
      weightLog.unshift({date:new Date().toISOString(),weight:format(weight),note:''});
      persistWeightLog('已记录');renderWeightLog();
    }
    function weightLogCsv() {
      const cell=value=>'"'+String(value).replace(/"/g,'""')+'"';
      const note=value=>/^\s*[=+\-@]/.test(value)?"'"+value:value;
      return '\ufeff'+[['日期','重量 (g)','说明'],...weightLog.map(record=>[logDate(record.date),record.weight,note(record.note)])].map(row=>row.map(cell).join(',')).join('\r\n')+'\r\n';
    }
    function exportWeightLog() {
      if(!weightLog.length)return;
      const url=URL.createObjectURL(new Blob([weightLogCsv()],{type:'text/csv;charset=utf-8'})),link=document.createElement('a'),date=new Date();
      const stamp=[date.getFullYear(),String(date.getMonth()+1).padStart(2,'0'),String(date.getDate()).padStart(2,'0')].join('-');
      link.href=url;link.download=`称重记录-${stamp}.csv`;document.body.append(link);link.click();link.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);
    }
    function message(text,error=false) {
      el('status').textContent=text;el('status').classList.toggle('error',error);
      el('status').classList.toggle('hidden',!text);
      if (error) errorUntil=Date.now()+4000;
    }
    function enqueueRequest(action) {
      const run=requestQueue.then(action);requestQueue=run.catch(()=>{});return run;
    }
    async function post(path,values={}) {
      if (busy) return false;
      busy=true;clearTimeout(pollTimer);updateScaleControls();renderState();renderRecords();
      try {
        return await enqueueRequest(async()=>{
          const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),12000);
          let result='';
          try {
            const response=await fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(values),signal:controller.signal});
            result=await response.text();
            if (!response.ok) throw new Error(result);
          } finally { clearTimeout(timeout); }
          errorUntil=0;
          await refreshOnce(path!=='/api/mode');
          if(result && result!=='OK'){message(result);errorUntil=Date.now()+4000;}
          return true;
        });
      } catch(error) {message(error.name==='AbortError'?'请求超时，操作结果未确认，请待连接恢复后核对':error.message,true);return false;}
      finally {busy=false;updateScaleControls();renderState();renderRecords();schedulePoll();}
    }
    function closeEditor() {editId=null;el('editForm').classList.add('hidden');}
    function renderRecords() {
      const list=el('spoolList');list.replaceChildren();
      el('count').textContent=listLoaded?`(${records.length})`:'';
      if (!records.length) {
        const empty=document.createElement('p');empty.className='note';
        empty.textContent=inventoryError || (listLoaded?'暂无耗材记录':'正在读取耗材记录…');list.append(empty);return;
      }
      for (const record of records) {
        const shown=displayReading(),active=shown?.state===1 && shown.id===record.id;
        const row=document.createElement('div');row.className='spool-row';
        const swatch=document.createElement('span');swatch.className='swatch';swatch.style.backgroundColor=record.color;
        const info=document.createElement('div');info.className='spool-text';
        const title=document.createElement('strong');title.textContent=`#${record.id} · ${materials[record.material] || '其他'}${active?' · 当前':''}`;
        const detail=document.createElement('small');
        const total=active?shown.value:record.total,net=remaining(record,total);
        detail.classList.toggle('warning',net==null);
        detail.textContent=`总重 ${format(total)} g · `+(net==null?'空盘重量待确认，请核对初始净重或编辑空盘重量':`余量 ${format(net)} g · 空盘 ${format(record.tare)} g`);
        info.append(title,detail);
        const select=document.createElement('button');select.className='secondary';select.textContent='选用';
        select.disabled=busy || !!readFailures || !modeReady || state?.state!==1 || state.id!==0;
        select.onclick=()=>post('/api/spools/select',{id:record.id});
        const edit=document.createElement('button');edit.className='secondary';edit.textContent='编辑';edit.disabled=busy||!modeReady;
        edit.onclick=()=>openEditor(record);
        const remove=document.createElement('button');remove.className='secondary';remove.textContent='删除';remove.disabled=busy||!modeReady||active;
        remove.onclick=()=>{if(confirm(`删除耗材 #${record.id} 的记录？`))post('/api/spools/delete',{id:record.id});};
        const actions=document.createElement('div');actions.className='spool-actions';actions.append(select,edit,remove);
        row.append(swatch,info,actions);list.append(row);
      }
    }
    function openEditor(record) {
      editId=record.id;el('editColor').value=record.color;el('editMaterial').value=String(record.material);
      el('editTare').value=record.tare??'';el('editTare').placeholder='输入空盘重量';
      el('editForm').classList.remove('hidden');el('editForm').scrollIntoView({block:'nearest',behavior:'smooth'});
    }
    function renderState() {
      if(!state)return;
      if(readFailures){updateScaleControls();el('create').disabled=true;return;}
      const shown=displayReading(),stale=state.state===98,spool=displayMode==='spool',valid=(shown.state===0||shown.state===1)&&Number.isFinite(shown.value),empty=spool&&shown.state===0;
      el('spoolMode').setAttribute('aria-pressed',String(spool));el('scaleMode').setAttribute('aria-pressed',String(!spool));
      el('inventorySection').classList.toggle('hidden',!spool);el('scaleSection').classList.toggle('hidden',spool);el('grossLine').classList.toggle('hidden',!spool);
      el('measurements').classList.toggle('hidden',empty);el('noSpool').classList.toggle('hidden',!empty);
      el('calibrationLink').classList.toggle('hidden',state.state!==99);
      updateScaleControls();
      el('create').disabled=busy||!modeReady||!spool||state.state!==1||state.id!==0||!listLoaded||!!inventoryError||records.length>=32;
      el('online').textContent='设备在线';el('gross').textContent=valid?format(shown.value):'--';
      let reading=null,status='',label='当前耗材余量';
      if(state.state===99)status='设备未校准，暂时无法显示重量';
      else if(valid&&!spool)reading=shown.value-localTare;
      else if(valid&&shown.state===1) {
        const record=records.find(item=>item.id===shown.id);
        if(!shown.id)status='等待重量稳定；未识别时可新建或选取历史耗材';
        else if(!record)status=`耗材 #${shown.id} 已确认，正在同步记录`;
        else {reading=remaining(record,shown.value);if(reading==null)status='请编辑空盘重量后查看余量';}
      }
      el('readingLabel').classList.toggle('hidden',!spool);el('readingLabel').textContent=label;el('weight').textContent=format(reading);
      if(stale){
        message(lastGoodMode===displayMode&&lastGoodReading?'传感器异常，读数暂未更新；显示上次有效数据':'传感器异常，暂无有效重量，请检查 HX711 连接',true);
        errorUntil=0; // Clear this live warning immediately when sampling recovers.
      }
      else if(!busy&&Date.now()>errorUntil)message(spool&&inventoryError?`${status}${status?'；':''}${inventoryError}`:status,spool&&!!inventoryError);
    }
    async function getJson(path) {
      const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),6000);
      try {
        const response=await fetch(path,{cache:'no-store',signal:controller.signal});
        if(!response.ok)throw new Error('读取失败');
        const result=await response.json();
        if(path==='/api/reading'){
          const mode=response.headers?.get('X-Scale-Mode');
          if(mode==='spool'||mode==='scale'){displayMode=mode;modeReady=true;}
        }
        return result;
      } finally {clearTimeout(timeout);}
    }
    async function loadRecords(force=false) {
      const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),6000);
      try {
        const response=await fetch('/api/spools',{cache:'no-store',headers:listTag&&!force?{'If-None-Match':listTag}:{},signal:controller.signal});
        if(response.status!==304) {
          if(!response.ok)throw new Error((await response.text())||'耗材记录读取失败');
          const next=await response.json();records=next;listLoaded=true;listTag=response.headers?.get('ETag')||'';
        }
        inventoryError='';nextListAt=Date.now()+60000;
        if(editId!==null&&!records.some(record=>record.id===editId))closeEditor();
      } catch(error) {
        inventoryError=error.name==='AbortError'?'耗材记录读取超时':error.message;
        nextListAt=Date.now()+60000;
      } finally {clearTimeout(timeout);}
    }
    async function refreshOnce(forceList=false) {
      try {
        state=await getJson('/api/reading');
        if(lastGoodMode!==displayMode||state.state===99)lastGoodReading=null;
        if((state.state===0||state.state===1)&&Number.isFinite(state.value)){
          lastGoodReading={...state};lastGoodMode=displayMode;
        }
        if(readFailures>=3)errorUntil=0;
        readFailures=0;renderState();
        if(forceList || (displayMode==='spool'&&Date.now()>=nextListAt))await loadRecords(forceList);
        renderState();renderRecords();readFailures=0;return true;
      } catch(error) {
        readFailures=Math.min(readFailures+1,3);
        if(readFailures>=3){
          el('online').textContent='连接中断';el('weight').textContent='--';el('gross').textContent='--';message('设备连接中断',true);
        }
        updateScaleControls();el('create').disabled=true;renderRecords();return false;
      }
    }
    function refresh(){return busy?Promise.resolve(false):enqueueRequest(()=>refreshOnce());}
    async function changeMode(mode){
      if(busy||(modeReady&&mode===displayMode))return;
      modeReady=false;await post('/api/mode',{mode});
    }
    el('spoolMode').onclick=()=>changeMode('spool');
    el('scaleMode').onclick=()=>changeMode('scale');
    el('create').onclick=async()=>{if(await post('/api/spools/create')){const record=records.find(item=>item.id===state?.id);if(record)openEditor(record);}};
    el('ordinaryTare').onclick=()=>{if(!busy&&scaleWeight()!=null){localTare=state.value;renderState();}};
    el('recordWeight').onclick=recordWeight;
    el('exportWeightLog').onclick=exportWeightLog;
    el('cancelEdit').onclick=closeEditor;
    el('editForm').onsubmit=async event=>{
      event.preventDefault();if(editId==null||busy)return;
      const savedId=editId,values={id:savedId,color:el('editColor').value,material:el('editMaterial').value};
      if(el('editTare').value.trim()!=='')values.tare=el('editTare').value;
      if(await post('/api/spools/update',values)&&editId===savedId)closeEditor();
    };
    function schedulePoll(){clearTimeout(pollTimer);pollTimer=setTimeout(poll,3000);}
    async function poll(){pollTimer=null;if(busy)return;await refresh();if(!busy)schedulePoll();}
    loadWeightLog();poll();
  </script>
</body>
</html>
)HTML";
