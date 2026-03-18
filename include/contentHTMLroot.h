#ifndef WIFI_CONTENT_HTML_ROOT_h
#define WIFI_CONTENT_HTML_ROOT_h


const char contentHTMLroot[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head>
  <meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <style>
    *{margin:0; padding:0; box-sizing:border-box;}
    body{display:flex; justify-content:center; align-items:center; font-family:sans-serif; background: #f4f4f9; padding:20px 15px;  min-height:100vh;}
    .text-center { text-align: center; margin: 5px 0; }
    .container { width:100%; max-width:500px; margin:auto; text-align:center; display:flex; 
             flex-direction:column; align-items:center; padding:20px 15px; background:white; 
             border-radius:10px; box-shadow:0 2px 10px rgba(0,0,0,0.1); }
    button { padding: 6px 12px; border:none; border-radius: 8px; font-size:16px; font-weight: 600; cursor:pointer; transition: all 0.3s;}
    .action-buttons { display:flex; gap:10px; margin-top: 20px; margin-bottom: 4px;}
    .save-btn { width:100%; background: #26de81; color: white;}
    .save-btn:hover { background: #20bf6b; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(38, 222, 129, 0.4);}
    .reset-btn { width:60%; background: #fc5c65; color:white;}
    .reset-btn:hover { background: #eb3b5a; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(252, 92, 101, 0.4);}
    .message { text-align:center; padding:12px; border-radius:8px; margin-bottom:20px; font-weight:500; display:none;}
    .message.success { background: #d4edda; color: #155724; display:block;}
    .message.error { background: #f8d7da; color: #721c24; display:block;}
    .options-section { width:100%; background:#f8f9fa; border:2px solid #e0e0e0; border-radius:8px; padding:15px; margin-bottom:20px; }
    .options-title { font-weight:700; font-size:16px; color:#333; margin-bottom:12px; text-align:left; }
    .option-row { display:flex; align-items:center; gap:10px; margin-bottom:10px; }
    .option-label { font-size:14px; color:#666; min-width:100px; text-align:left; }
    .option-input { flex:1; padding:8px; border:2px solid #e0e0e0; border-radius:6px; font-size:14px; }
    .option-input:focus { outline:none; border-color:#667eea; box-shadow:0 0 0 2px rgba(102,126,234,0.1); }
    .option-checkbox { width:20px; height:20px; cursor:pointer; accent-color:#667eea; }
    .option-select { flex:1; padding:8px; border:2px solid #e0e0e0; border-radius:6px; font-size:14px; background:white; cursor:pointer; }
    .option-select:focus { outline:none; border-color:#667eea; box-shadow:0 0 0 2px rgba(102,126,234,0.1); }
    .color-picker { width: 50px; height: 30px; border: 1px solid #ccc; border-radius: 4px; background: none; cursor: pointer; padding: 2px; }
    .color-picker::-webkit-color-swatch-wrapper { padding: 0; }
    .color-picker::-webkit-color-swatch { border: none; border-radius: 2px; }
    input:disabled { opacity:0.6; }
    #clock { font-size: 24px; font-family: monospace; color: #007bff; }
   @media (max-width:400px) { 
      .option-row { flex-direction:column; align-items:flex-start; }
      .option-label { min-width:auto; }
    }
  </style>
  <title>[ESP32] Yellow Display Manager</title>
  </head>
  <body>
  <div class='container'>
  <h2><span id="deviceName">[...]</span></h2>
  <p style='color:#666;' class='text-center'>Firmware version: <span id="firmwareVersion">...</span></p>

  <div id="message" class="message"></div>

  <div class="options-section">
      <div class="options-title">Device Options</div>
    
      <div class="option-row">
        <label class="option-label" for="isResetDaily">Reset Daily:</label>
        <input type="checkbox" id="isResetDaily" class="option-checkbox" checked />

        <div style="display:flex; align-items:center; gap:2px;">
        <select id="resetHourSelect" class="option-select" style="flex:0.5; margin-right:0px;"></select><span style="margin-right:8px;">H</span>
        </div>
        <div style="display:flex; align-items:center; gap:2px;">
        <select id="resetMinuteSelect" class="option-select" style="flex:0.5; margin-right:0px;"></select><span style="margin-right:8px;">M</span>
        </div>

      </div>

      <div class="option-row">
        <label class="option-label" for="timezoneSelect">From Timezone:</label>
        <select id="timezoneSelect" class="option-select">
          <option value='0' >Zulu Universal Time (0)</option>
          <option value='1' >Europe/Berlin (+1)</option>
          <option value='2' >Europe/Helsinki (+2)</option>
          <option value='3' >Europe/Moscow (+3)</option>
          <option value='4' >Asia/Dubai (+4)</option>
          <option value='5' >Asia/Karachi (+5)</option>
          <option value='6' >Asia/Dhaka (+6)</option>
          <option value='7' >Asia/Jakarta (+7)</option>
          <option value='8' >Asia/Manila (+8)</option>
          <option value='9' >Asia/Tokyo (+9)</option>
          <option value='10'>Australia/Sydney (+10)</option>
          <option value='11'>Pacific/Noumea (+11)</option>
          <option value='12'>Pacific/Auckland (+12)</option>
          <option value='13'>Atlantic/Azores (-1)</option>
          <option value='14'>America/Noronha (-2)</option>
          <option value='15'>America/Araguaina (-3)</option>
          <option value='16'>America/Blanc-Sablon (-4)</option>
          <option value='17'>America/New_York (-5)</option>
          <option value='18'>America/Chicago (-6)</option>
          <option value='19'>America/Denver (-7)</option>
          <option value='20'>America/Los_Angeles (-8)</option>
          <option value='21'>America/Anchorage (-9)</option>
          <option value='22'>Pacific/Honolulu (-10)</option>
          <option value='23'>Pacific/Niue (-11)</option>
        </select>
      </div>
      <p>Resetting daily will provide better long-term stability.</p>
      
  </div>


  <div class="options-section">
      <div class="options-title">Visual Options</div>

      <div class="option-row">
        <label class="option-label" for="temperatureUnitSelect">Temperature Units:</label>
        <select id="temperatureUnitSelect" class="option-select">
          <option value='0' >Celsius</option>
          <option value='1' >Fahrenheit</option>
        </select>
      </div>

      <div class="option-row">
        <label class="option-label" for="temperatureDigit">Temperature Digits:</label>
        <input type="number" id="temperatureDigit" class="option-input" step='1' min='0' max='1' value='0'/>
      </div>

      <div class="option-row">
        <label class="option-label" for="windUnitSelect">Wind Units:</label>
        <select id="windUnitSelect" class="option-select">
          <option value='0' >Knots (kt)</option>
          <option value='1' >Miles per hour (mph)</option>
          <option value='2' >Kilometers per hour (kph)</option>
        </select>
      </div>

      <div class="option-row">
        <label class="option-label" for="windDigit">Wind Digits:</label>
        <input type="number" id="windDigit" class="option-input" step='1' min='0' max='1' value='0'/>
      </div>

      <div class="option-row">
        <label class="option-label" for="isDrawFrame">Draw Frame of Forecast location:</label>
        <input type="checkbox" id="isDrawFrame" class="option-checkbox" checked />
        <input type='color' id='colFrame' name='colFrame' value='#000000' class='color-picker'>
      </div>

  </div>

  <div class="options-section">
      <div class="options-title">API Parameters</div>
    
      <div class="option-row">
        <label class="option-label" for="apiUrlBase">API URL:</label>
        <input type="text" id="apiUrlBase" class="option-input" placeholder="e.g., https://api.weatherapi.com/v1/current.json?key=" />
      </div>

      <div class="option-row">
        <label class="option-label" for="apiKey">API Key:</label>
        <input type="text" id="apiKey" class="option-input" placeholder="e.g., 1234" />
      </div>

      <div class="option-row">
        <label class="option-label" for="apiUrlLocation">API url for location:</label>
        <input type="text" id="apiUrlLocation" class="option-input" placeholder="e.g., &aqi=no&q=" />
      </div>

      <div class="option-row">
        <label class="option-label" for="apiUrlForecast">API url for forecast:</label>
        <input type="text" id="apiUrlForecast" class="option-input" placeholder="e.g., &days=" />
      </div>

      <div class="option-row">
        <label class="option-label" for="apiLocation1">Location 1:</label>
        <input type="text" id="apiLocation1" class="option-input" placeholder="e.g., Seattle, zip, coord" />
      </div>
      <div class="option-row">
        <label class="option-label" for="apiLocation2">Location 2:</label>
        <input type="text" id="apiLocation2" class="option-input" placeholder="e.g., Seattle, zip, coord" />
      </div>

      <div class="option-row">
        <label class="option-label" for="apiForecastLocation">Forecast Location:</label>
        <select id="apiForecastLocation" class="option-select">
          <option value='1' >Location 1</option>
          <option value='2' >Location 2</option>
        </select>
      </div>

      <div class="action-buttons">
      <button class="reset-btn" id="resetBtnOptionsAPI">Reset API Options to Default</button>
      </div>
      <p>API Reference: <a href="https://www.weatherapi.com/api-explorer.aspx" target="_blank">https://www.weatherapi.com/api-explorer.aspx</a></p>
  </div>



  <div class="action-buttons">
      <button class="save-btn" id="saveBtn">Save</button>
  </div>
  

  <p id='clock' style='color:#666;' class='text-center'> Fetching time...</p>
  
  <script>
    // RGB565 (uint16) ↔ CSS #RRGGBB conversions for color picker ↔ server
    function rgb565ToHex(v) {
      const r = ((v >> 11) & 0x1F) << 3;
      const g = ((v >>  5) & 0x3F) << 2;
      const b = ( v        & 0x1F) << 3;
      return '#' + [r, g, b].map(c => c.toString(16).padStart(2, '0')).join('');
    }
    function hexToRgb565(hex) {
      const r = parseInt(hex.slice(1, 3), 16) >> 3;
      const g = parseInt(hex.slice(3, 5), 16) >> 2;
      const b = parseInt(hex.slice(5, 7), 16) >> 3;
      return (r << 11) | (g << 5) | b;
    }

    const saveBtn = document.getElementById('saveBtn');
    const resetBtnOptionsAPI = document.getElementById('resetBtnOptionsAPI');

    const messageEl = document.getElementById('message');
    const timezoneSelect = document.getElementById('timezoneSelect');
    const isResetDailyCheckbox = document.getElementById('isResetDaily');
    const resetHourSelect = document.getElementById('resetHourSelect');
    const resetMinuteSelect = document.getElementById('resetMinuteSelect');
    const temperatureUnitSelect = document.getElementById('temperatureUnitSelect');
    const temperatureDigit = document.getElementById('temperatureDigit');
    const windUnitSelect = document.getElementById('windUnitSelect');
    const windDigit = document.getElementById('windDigit');
    const isDrawFrameCheckbox = document.getElementById('isDrawFrame');
    const colorFrameSelect = document.getElementById('colFrame');

    const apiUrlBase = document.getElementById('apiUrlBase');
    const apiKey = document.getElementById('apiKey');
    const apiUrlLocation = document.getElementById('apiUrlLocation');
    const apiLocation1 = document.getElementById('apiLocation1');
    const apiLocation2 = document.getElementById('apiLocation2');
    const apiUrlForecast = document.getElementById('apiUrlForecast');
    const apiForecastLocation = document.getElementById('apiForecastLocation');
    

    let clockTimer;
    let options = {
      timezone: 0,
      isResetDaily: true,
      resetHour: 0,
      resetMinute: 0,
      tempUnit: 0,
      tempDigit: 0,
      windUnit: 0,
      windDigit: 0,
      isDrawFrame: false,
      colorFrame: 0x0
    };
    let optionsAPI = {
      apiUrlBase: '',
      apiKey: '',
      apiUrlLocation: '',
      apiLocation1: '',
      apiLocation2: '',
      apiUrlForecast: '',
      apiForecastLocation: 1
    };

  function fetchTime() {
    fetch('/time')
      .then(response => {
        console.log('fetchTime status:', response.status);
        return response.text();
      })
      .then(data => {
        if (data !== "") {
          console.log('Time fetched:', data);
          document.getElementById("clock").innerText = data;
        }
      })
      .catch(err => console.warn('Clock Fetch error:', err));
  }

  function showMessage(text, type) {
      messageEl.textContent = text;
      messageEl.className = `message ${type}`;
      setTimeout(() => {
          messageEl.className = 'message';
      }, 5000);
  }

  function updateField(id, value) {
    const el = document.getElementById(id);
    if (el && value !== undefined) el.textContent = value;
  }

  async function loadPreferences() {
    try {
      const response = await fetch('/fetchPreferences');
      if (response.ok) {
        const data = await response.json();
        
        // Update device info
        document.title = data.deviceName + " Display Options";
        updateField('deviceName', data.deviceName);
        updateField('firmwareVersion', data.fwVersion);

        timezoneSelect.value = data.timezone ?? 0;
        isResetDailyCheckbox.checked = data.isResetDaily ?? true;
        resetHourSelect.value = data.resetHour ?? 0;
        resetMinuteSelect.value = data.resetMinute ?? 0;
        temperatureUnitSelect.value = data.tempUnit ?? 0;
        temperatureDigit.value = data.tempDigit ?? 0;
        windUnitSelect.value = data.windUnit ?? 0;
        windDigit.value = data.windDigit ?? 0;
        isDrawFrameCheckbox.checked = data.isDrawFrame ?? true;
        colorFrameSelect.value = rgb565ToHex(data.colorFrame ?? 0x07FF);

        apiUrlBase.value = data.api_urlBase || '';
        apiKey.value = data.api_key || '';
        apiUrlLocation.value = data.api_urlLocation || ''; 
        apiLocation1.value = data.api_location1 || '';
        apiLocation2.value = data.api_location2 || '';
        apiUrlForecast.value = data.api_urlForecast || '';
        apiForecastLocation.value = data.api_ForecastLoc || '';

      } else {
        throw new Error('Server response not OK');
      }
    } catch (error) {
      console.warn('Failed to fetch from server, loading from localStorage:', error);
      
      // Fallback to localStorage
      const savedOptions = localStorage.getItem('Options');
      const savedOptionsAPI = localStorage.getItem('OptionsAPI');
    if (savedOptions) {
        try {
            options = JSON.parse(savedOptions);
            timezoneSelect.value = options.timezone ?? 0;
            isResetDailyCheckbox.checked = options.isResetDaily ?? true;
            resetHourSelect.value = options.resetHour ?? 0;
            resetMinuteSelect.value = options.resetMinute ?? 0;
            temperatureUnitSelect.value = options.tempUnit ?? 0;
            temperatureDigit.value = options.tempDigit ?? 0;
            windUnitSelect.value = options.windUnit ?? 0;
            windDigit.value = options.windDigit ?? 0;
            isDrawFrameCheckbox.checked = options.isDrawFrame ?? true;
            colorFrameSelect.value = rgb565ToHex(options.colorFrame ?? 0x07FF);
        } catch (e) {
            console.warn('Error parsing saved options:', e);
        }
    }
    if (savedOptionsAPI) {
        try {
            optionsAPI = JSON.parse(savedOptionsAPI);
            apiUrlBase.value = optionsAPI.apiUrlBase || '';
            apiKey.value = optionsAPI.apiKey || '';
            apiUrlLocation.value = optionsAPI.apiUrlLocation || '';
            apiLocation1.value = optionsAPI.apiLocation1 || '';
            apiLocation2.value = optionsAPI.apiLocation2 || '';
            apiUrlForecast.value = optionsAPI.apiUrlForecast || '';
            apiForecastLocation.value = optionsAPI.apiForecastLocation || '';
        } catch (e) {
            console.warn('Error parsing saved optionsAPI:', e);
        }
    }
    } 
  }


  async function savePreferences() {

      optionsAPI.apiUrlBase = apiUrlBase.value.trim();
      optionsAPI.apiKey = apiKey.value.trim();
      optionsAPI.apiUrlLocation = apiUrlLocation.value.trim();
      optionsAPI.apiLocation1 = apiLocation1.value.trim();
      optionsAPI.apiLocation2 = apiLocation2.value.trim();
      optionsAPI.apiUrlForecast = apiUrlForecast.value.trim();
      optionsAPI.apiForecastLocation = parseInt(apiForecastLocation.value);
      

      
      options.timezone = parseInt(timezoneSelect.value);
      options.isResetDaily = isResetDailyCheckbox.checked;
      options.resetHour = parseInt(resetHourSelect.value);
      options.resetMinute = parseInt(resetMinuteSelect.value);
      options.tempUnit = parseInt(temperatureUnitSelect.value);
      options.tempDigit = parseInt(temperatureDigit.value);
      options.windUnit = parseInt(windUnitSelect.value);
      options.windDigit = parseInt(windDigit.value);
      options.isDrawFrame = isDrawFrameCheckbox.checked;
      options.colorFrame = hexToRgb565(colorFrameSelect.value);

      try {
        const response = await fetch('/savePreferences', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ 
                timezone: options.timezone,
                isResetDaily: options.isResetDaily,
                resetHour: options.resetHour,
                resetMinute: options.resetMinute,
                tempUnit: options.tempUnit,
                tempDigit: options.tempDigit,
                windUnit: options.windUnit,
                windDigit: options.windDigit,
                isDrawFrame: options.isDrawFrame,
                colorFrame: options.colorFrame,

                apiUrlBase: optionsAPI.apiUrlBase,
                apiKey: optionsAPI.apiKey,
                apiUrlLocation: optionsAPI.apiUrlLocation,
                apiLocation1: optionsAPI.apiLocation1,
                apiLocation2: optionsAPI.apiLocation2,
                apiUrlForecast: optionsAPI.apiUrlForecast,
                apiForecastLoc: optionsAPI.apiForecastLocation
          })
        });
        if (response.ok) {
          localStorage.setItem('Options', JSON.stringify(options));
          localStorage.setItem('OptionsAPI', JSON.stringify(optionsAPI));
          showMessage('List saved successfully!', 'success');
        } else {
          showMessage('Server rejected the data', 'error');
        }
      } catch (error) {
        console.warn('Save error:', error);
        showMessage('Connection lost to ESP32', 'error');
      }
      
  }



  function resetOptionsAPI() {

      if (confirm('Are you sure you want to reset the API Options to Default?')) {
          optionsAPI = {
              apiUrlBase: 'https://api.weatherapi.com/v1/current.json?key=',
              apiKey: '1234',
              apiUrlLocation: '&aqi=no&q=',
              apiLocation1: 'auto:ip',
              apiLocation2: 'auto:ip',
              apiUrlForecast: '&days=',
              apiForecastLocation: '1'
          };

          apiUrlBase.value = optionsAPI.apiUrlBase || '';
          apiKey.value = optionsAPI.apiKey || '';
          apiUrlLocation.value = optionsAPI.apiUrlLocation || '';
          apiLocation1.value = optionsAPI.apiLocation1 || '';
          apiLocation2.value = optionsAPI.apiLocation2 || '';
          apiUrlForecast.value = optionsAPI.apiUrlForecast || '';
          apiForecastLocation.value = optionsAPI.apiForecastLocation || '';

          localStorage.setItem('OptionsAPI', JSON.stringify(optionsAPI));
          showMessage('API Options reset to default successfully', 'success');
      }
  }

  function toggleResetDropdowns() {
    const isDisabled = !isResetDailyCheckbox.checked;
    resetHourSelect.disabled = isDisabled;
    resetMinuteSelect.disabled = isDisabled;
    timezoneSelect.disabled = isDisabled;
  }

  function toggleDrawFrame() {
    const isDisabled = !isDrawFrameCheckbox.checked;
    colorFrameSelect.disabled = isDisabled;
  }

  // Event listeners
  saveBtn.addEventListener('click', savePreferences);
  resetBtnOptionsAPI.addEventListener('click', resetOptionsAPI);
  isResetDailyCheckbox.addEventListener('change', toggleResetDropdowns);
  isDrawFrameCheckbox.addEventListener('change', toggleDrawFrame);

  // Generate hour/minute options dynamically (saves ~600 bytes of flash vs hardcoded options)
  for (let i = 0; i < 24; i++) {
    const o = document.createElement('option');
    o.value = i; o.textContent = String(i).padStart(2, '0');
    resetHourSelect.appendChild(o);
  }
  for (let i = 0; i < 60; i += 5) {
    const o = document.createElement('option');
    o.value = i; o.textContent = String(i).padStart(2, '0');
    resetMinuteSelect.appendChild(o);
  }

  // Load data on page load
  loadPreferences();
  toggleResetDropdowns();
  clockTimer = setInterval(fetchTime, 1000);

  </script>
  </body></html>
  )rawliteral";


#endif