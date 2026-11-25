#pragma once

constexpr char SETTINGS_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>设置 · 耗材秤</title>
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
    h2 { margin:0 0 14px; font-size:17px; }
    .section { padding:22px 0; border-bottom:1px solid var(--line); }
    fieldset { border:0; margin:0; padding:0; min-width:0; }
    .fields { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:18px 24px; }
    label { display:block; color:var(--muted); font-size:13px; }
    .input-unit { display:flex; align-items:center; border:1px solid #9cb2a9; border-radius:var(--radius); background:var(--white); margin-top:5px; overflow:hidden; }
    .input-unit:focus-within { outline:2px solid var(--green); outline-offset:1px; }
    input { min-width:0; min-height:42px; font:inherit; color:var(--ink); }
    .input-unit input { min-height:40px; width:100%; border:0; outline:none; padding:0 10px; background:transparent; font-size:16px; }
    .input-unit span { padding:0 10px; color:var(--muted); font-size:12px; white-space:nowrap; }
    .check { display:flex; align-items:center; gap:8px; margin-top:14px; min-height:32px; }
    .check input { min-height:0; width:16px; height:16px; margin:0; }
    details { margin-top:14px; font-size:13px; color:var(--muted); }
    summary { cursor:pointer; }
    details p { line-height:1.7; overflow-wrap:anywhere; }
    .network-link { display:block; margin-top:8px; color:var(--green); font-size:13px; }
    .intro { margin:12px 0 0; font-size:13px; line-height:1.7; color:var(--muted); }
    button { min-height:42px; padding:0 14px; border:1px solid var(--green); border-radius:var(--radius); background:var(--green); color:white; font:inherit; font-size:14px; cursor:pointer; transition:background var(--motion); }
    button:hover:not(:disabled) { background:#10523f; }
    button.secondary { background:var(--white); color:var(--ink); border-color:var(--line); }
    button.secondary:hover:not(:disabled) { background:#e9efec; }
    button:disabled { opacity:.5; cursor:default; }
    button:focus-visible,a:focus-visible,input[type=file]:focus-visible { outline:2px solid var(--green); outline-offset:3px; }
    .row { display:flex; flex-wrap:wrap; align-items:end; gap:10px; margin-top:14px; }
    .row label { width:220px; max-width:100%; }
    input[type=file] { width:100%; max-width:420px; padding:8px 0; font-size:13px; }
    progress { display:block; width:min(100%,420px); height:8px; margin-top:14px; accent-color:var(--green); }
    .message { font-size:13px; line-height:1.6; margin:10px 0 0; color:var(--green); }
    .message:empty { display:none; }
    .error { color:var(--red)!important; }
    .hidden { display:none!important; }
    .savebar { display:flex; align-items:center; gap:18px; padding:20px 0; border-bottom:1px solid var(--line); }
    .savebar strong:empty,.savebar p:empty { display:none; }
    .savebar button { flex-shrink:0; }
    .savebar strong { font-size:13px; font-weight:500; }
    .savebar p { margin:4px 0 0; color:var(--muted); font-size:12px; line-height:1.6; }
    @media(max-width:600px) { .fields { gap:18px 14px; } .row label { width:100%; } .savebar { align-items:start; gap:12px; } }
  </style>
</head>
<body>
<main>
  <header>
    <h1>设置</h1>
    <a class="nav-button" href="/">返回</a>
  </header>

  <form id="settingsForm" novalidate>
    <fieldset id="settingsFields" disabled>
      <section class="section" aria-labelledby="spoolHeading">
        <h2 id="spoolHeading">耗材识别</h2>
        <div class="fields">
          <label for="nominalG">新耗材初始净重<div class="input-unit"><input id="nominalG" type="number" step="0.1" inputmode="decimal"><span>g</span></div></label>
          <label for="matchToleranceG">自动匹配容差<div class="input-unit"><input id="matchToleranceG" type="number" step="0.1" inputmode="decimal"><span>g</span></div></label>
          <label for="emptyLimitG">空台阈值<div class="input-unit"><input id="emptyLimitG" type="number" step="0.1" inputmode="decimal"><span>g</span></div></label>
          <label for="loadedLimitG">上料阈值<div class="input-unit"><input id="loadedLimitG" type="number" step="0.1" inputmode="decimal"><span>g</span></div></label>
        </div>
      </section>
      <section class="section" aria-labelledby="checkpointHeading">
        <h2 id="checkpointHeading">自动保存</h2>
        <div><div class="fields">
          <label for="intervalMinutes">自动保存间隔<div class="input-unit"><input id="intervalMinutes" type="number" step="1" inputmode="numeric"><span>分钟</span></div></label>
          <label for="minimumChangeG">最小重量变化<div class="input-unit"><input id="minimumChangeG" type="number" step="0.1" inputmode="decimal"><span>g</span></div></label>
        </div></div>
      </section>
      <section class="section" aria-labelledby="wifiHeading">
        <h2 id="wifiHeading">Wi-Fi</h2>
        <div class="fields">
          <label for="wifiSsid">Wi-Fi 名称<div class="input-unit"><input id="wifiSsid" type="text" autocomplete="off"></div></label>
          <label for="wifiPassword">Wi-Fi 密码<div class="input-unit"><input id="wifiPassword" type="password" autocomplete="new-password" placeholder="留空保留原密码"></div></label>
        </div>
        <label class="check"><input id="wifiOpen" type="checkbox">无密码网络</label>
        <details><summary>备用热点</summary><p><span id="apName"></span> · 开放热点<br><a href="http://192.168.4.1/">http://192.168.4.1/</a></p></details>
      </section>
    </fieldset>
  </form>
  <div class="savebar"><button id="saveSettings" type="submit" form="settingsForm" disabled>保存设置</button><div><strong id="saveState" role="status" aria-live="polite">正在读取设置…</strong><p id="settingsMessage"></p><a id="deviceLink" class="network-link hidden">打开设备新地址</a><a id="reload" href="/settings" class="message hidden">重新打开设置页</a></div></div>
  <section class="section" aria-labelledby="calHeading">
    <h2 id="calHeading">称台校准</h2>
    <p class="intro">先清空称台归零，再放上已知重量的砝码进行校准。操作立即生效。</p>
    <div class="row"><button id="baseTare" class="secondary" type="button" disabled>空台归零</button><label for="known"><div class="input-unit"><input id="known" aria-label="砝码重量（克）" type="number" step="any" inputmode="decimal" placeholder="例如 500"><span>g</span></div></label><button id="calibrate" class="secondary" type="button" disabled>校准秤</button></div>
    <p id="calStatus" class="message" role="status" aria-live="polite"></p>
  </section>
  <section class="section" aria-labelledby="updateHeading">
    <h2 id="updateHeading">固件更新</h2>
    <form id="uploadForm" novalidate><div class="row"><input id="firmware" name="firmware" type="file" accept=".bin,application/octet-stream" aria-label="固件文件"><button id="upload" class="secondary" type="submit" disabled>上传并更新</button></div><progress id="progress" class="hidden" value="0" aria-label="上传进度"></progress><p id="uploadMessage" class="message" role="status" aria-live="polite"></p></form>
  </section>
</main>
<script>
  const el=id=>document.getElementById(id);
  const keys=['nominalG','matchToleranceG','emptyLimitG','loadedLimitG','intervalMinutes','minimumChangeG'];
  let loaded=false,busy='',saved='',settingsToken='';
  const values=()=>({...Object.fromEntries(keys.map(key=>[key,el(key).value])),wifiSsid:el('wifiSsid').value,wifiPassword:el('wifiPassword').value,wifiOpen:String(el('wifiOpen').checked)});
  const fingerprint=()=>JSON.stringify([...keys.map(key=>Number(el(key).value)),el('wifiSsid').value,el('wifiPassword').value,el('wifiOpen').checked]);
  function controls() {
    el('settingsFields').disabled=!loaded||!!busy;
    el('wifiPassword').disabled=!loaded||!!busy||el('wifiOpen').checked;
    el('saveSettings').disabled=!loaded||!!busy||fingerprint()===saved;
    el('baseTare').disabled=!loaded||!!busy;
    el('calibrate').disabled=!loaded||!!busy;
    el('known').disabled=!!busy;
    el('firmware').disabled=!!busy;
    el('upload').disabled=!loaded||!!busy||!el('firmware').files.length;
  }
  function note(title,text,error=false) {
    el('saveState').textContent=title;el('settingsMessage').textContent=text;
    el('saveState').classList.toggle('error',error);
  }
  async function request(path,options={}) {
    const controller=new AbortController();
    const timer=setTimeout(()=>controller.abort(),12000);
    try {
      const response=await fetch(path,{...options,cache:'no-store',signal:controller.signal});
      const body=await response.text();
      if (!response.ok) throw new Error(body||'设备请求失败');
      return body;
    } catch(error) {throw new Error(error.name==='AbortError'?'请求超时，请检查设备连接':error.message);}
    finally {clearTimeout(timer);}
  }
  async function loadSettings() {
    try {
      const config=JSON.parse(await request('/api/settings'));
      for(const key of keys) el(key).value=config[key]??'';
      el('wifiSsid').value=config.wifiSsid??'';el('wifiOpen').checked=!!config.wifiOpen;
      el('wifiPassword').value='';settingsToken=config.settingsToken??'';
      el('apName').textContent=config.apSsid??'';el('deviceLink').href=config.deviceUrl||'/settings';
      el('calStatus').textContent=config.calibrated?'已校准':'尚未校准，需完成归零和砝码校准后才能显示重量';
      saved=fingerprint();loaded=true;
      note('','');
    } catch(error) {note('未能读取设置',error.message,true);el('reload').classList.remove('hidden');}
    controls();
  }
  el('settingsForm').oninput=()=>{
    if(el('wifiOpen').checked) el('wifiPassword').value='';
    note(fingerprint()===saved?'':'有未保存的修改','');controls();
  };
  el('settingsForm').onsubmit=async event=>{
    event.preventDefault();
    if(!loaded||busy) return;
    const body=new URLSearchParams(values());
    busy='save';controls();el('saveSettings').textContent='保存中…';note('正在保存','请稍候');
    try {
      const result=JSON.parse(await request('/api/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Settings-Token':settingsToken},body}));
      el('wifiPassword').value='';saved=fingerprint();
      if(result.networkChanged) {
        busy='network';note('Wi-Fi 已保存，正在连接','若连接失败，可通过备用热点重新配置。');
        el('deviceLink').classList.remove('hidden');el('reload').classList.remove('hidden');
      } else note('设置已保存','');
    } catch(error) {note('未能确认保存',error.message,true);el('reload').classList.remove('hidden');}
    finally {if(busy==='save') busy='';el('saveSettings').textContent='保存设置';controls();}
  };
  async function calibrate(path,params={}) {
    if(!loaded||busy) return;
    busy='calibration';controls();el('calStatus').textContent='正在读取称台，请保持稳定…';el('calStatus').classList.remove('error');
    try {
      await request(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(params)});
      el('calStatus').textContent=path==='/api/calibrate'?'校准完成，可以取下砝码了':'归零完成，请放上已知重量的砝码';
    } catch(error) {el('calStatus').textContent=error.message;el('calStatus').classList.add('error');}
    finally {busy='';controls();}
  }
  el('baseTare').onclick=()=>calibrate('/api/base-zero');
  el('calibrate').onclick=()=>{
    return calibrate('/api/calibrate',{grams:el('known').value});
  };
  el('firmware').onchange=controls;
  el('uploadForm').onsubmit=event=>{
    event.preventDefault();if(!loaded||busy) return;
    const file=el('firmware').files[0];
    if(!file) return;
    busy='upload';controls();el('progress').classList.remove('hidden');el('progress').value=0;el('uploadMessage').textContent='上传中，请保持设备供电…';el('uploadMessage').classList.remove('error');
    const xhr=new XMLHttpRequest();xhr.open('POST','/api/update');xhr.timeout=120000;
    xhr.upload.onprogress=event=>{if(event.lengthComputable) el('progress').value=Math.round(event.loaded*100/event.total);};
    const fail=message=>{busy='';controls();el('uploadMessage').textContent=message;el('uploadMessage').classList.add('error');el('progress').classList.add('hidden');};
    xhr.onload=()=>{
      if(xhr.status===200) {busy='restarting';controls();el('progress').value=100;el('uploadMessage').textContent='更新成功，设备正在重启。稍后重新打开本页。';el('reload').classList.remove('hidden');}
      else fail(xhr.responseText||'更新失败，请重试');
    };
    xhr.onerror=xhr.ontimeout=()=>fail('连接中断，请待设备恢复后确认更新结果');
    const form=new FormData();form.append('firmware',file,file.name);xhr.send(form);
  };
  loadSettings();
</script>
</body>
</html>
)HTML";
