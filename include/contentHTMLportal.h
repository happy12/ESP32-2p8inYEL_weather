#ifndef CAPTIVE_PORTAL_HTML_h
#define CAPTIVE_PORTAL_HTML_h


//use %% for the normal usage (such as width:60%%) to avoid confusion with the variable processor (such as %CHARTAXIS_HUMID_MAX%)
const char contentHTMLportal[] PROGMEM = R"rawliteral(
    <!DOCTYPE html><html><head><title>WiFi Setup</title>
      <meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>
      <style>
      body { font-family: sans-serif; font-size: 1.2rem; padding: 20px; line-height: 1.5; }
      input { display: block; width: auto; min-width: 90vw; max-width: 90vw; box-sizing: border-box; margin-bottom: 20px; 
        margin-left: 0; margin-right: 0; padding: 15px; font-size: 1.1rem; border: 1px solid #ccc; border-radius: 8px; }
      form { display: flex; flex-direction: column; }
      select { display: inline-block; width: auto; margin-bottom: 20px; height: 40px; line-height: 1; padding: 5px 10px 5px 10px;
         font-size: 1.1rem; border: 1px solid #ccc; vertical-align: bottom; border-radius: 8px; box-sizing: border-box; }
      select option {vertical-align: bottom; padding: 5px;}
      input[type='submit'] { background-color: #007bff; color: white; border: none; cursor: pointer; }
      .refresh-btn { display: block; width: auto; padding: 15px; font-size: 1.1rem; border-radius: 8px; cursor: pointer; 
        background-color: #6c757d; color: white; border: none; margin-top: 10px; }
      </style>
      <script defer>
        function togglePW() {
          var x = document.getElementById('pw');
          if (x.type === 'password') { x.type = 'text'; }
          else { x.type = 'password'; }
        }
        async function refreshWiFi() {
          const btn = document.querySelector('.refresh-btn');
          const select = document.querySelector("select[name='ssid']");
          btn.innerText = "Scanning...";
          btn.disabled = true;

          try {
            const response = await fetch('/scanWifi');
            if (!response.ok) throw new Error("Server Busy");
            const html = await response.text();
            select.innerHTML = html;
            btn.innerText = "Refresh WiFi Network List";
          } catch (err) {
            console.error(err);
            btn.innerText = "Error Scanning Wifi (try again?)";
          }
          finally {
            btn.disabled = false; // Always re-enable the button
          }
        }
        window.onload = function() {
          var ua = navigator.userAgent;
          var type = 'Generic User';
          if (/iPhone|iPad|iPod/.test(ua)) type = 'iOS User';
          else if (/Android/.test(ua)) type = 'Android User';
          document.getElementById('device-display').innerText = type;
          refreshWiFi();
        }
      </script>
      </head><body>

      <h2 style='text-align: center;'>Network Wifi Setup (<span id='device-display'>...</span>)</h2>
      <h2>Select Your Wi-Fi</h2>

      <form action='/save' method='POST'>

      SSID:<br><select name='ssid'>...</select>
    
      Password:<br><input type='password' id='pw' name='password' placeholder='Password'>
    
      <div style='margin-bottom:20px;'>
        <input type='checkbox' id='show' onclick='togglePW()' style='display:inline; width:auto; margin-bottom:0;'>
        <label for='show' style='font-size:1rem; margin-left:10px;'>Show Password</label>
      </div>

      <input type='submit' value='Save and Connect'>
      </form>

    <button type="button" class='refresh-btn' onclick='refreshWiFi()' style='margin-top:10px;'>Refresh WiFi Network List</button>
    </body></html>
    )rawliteral";

#endif