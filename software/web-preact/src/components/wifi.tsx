import { Component } from 'preact';
import { useEffect, useRef, useState } from 'preact/hooks';

import './comp.css';
import { StatusAlertService } from 'react-status-alert';

const MANUAL_SSID = '__manual__';

// Rough signal bars for the dropdown label, using the same thresholds as the knob's status icon.
const signalLabel = (rssi) => {
  if (rssi >= -70) return '▂▄▆█';
  if (rssi >= -85) return '▂▄▆';
  if (rssi >= -100) return '▂▄';
  return '▂';
};

const WiFiComp = () => {
  const [disabled, setDisabled] = useState(false);

  const [networks, setNetworks] = useState([]);
  const [scanning, setScanning] = useState(false);
  const [manual, setManual] = useState(false);

  const scan = async () => {
    setScanning(true);
    try {
      const response = await fetch(`http://${window.location.hostname}/wifi/scan`);
      const found = response.ok ? await response.json() : [];
      setNetworks(found);
      if (found.length == 0) {
        setManual(true);
      } else if (!manual && !found.some((n) => n.ssid == ssid)) {
        setSsid(found[0].ssid);
      }
    } catch (error) {
      setManual(true);
    }
    setScanning(false);
  };

  useEffect(() => {
    scan();
  }, []);

  const [ssid, setSsid] = useState('');
  const [password, setPswd] = useState('');

  const [status, setStatus] = useState('');

  // Handle form submission
  const handleSubmit = async (e) => {
    e.preventDefault();

    const data = { ssid, password };

    try {
      setDisabled(true);
      const response = await fetch(`http://${window.location.hostname}/wifi`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(data),
      });

      if (response.ok) {
        StatusAlertService.showSuccess(
          <div>
            <h3>WiFi Connected</h3>
          </div>,
          {
            autoHide: true,
            withIcon: false,
            withCloseIcon: false,
            removeAllBeforeShow: true,
            autoHideTime: 2500,
          },
        );

        var json = await response.json();
        if (json.redirect == 'spotify') {
          console.log(json.data);

          //   window.location.href = '/?' + json.redirect;
          window.location.href = `http://${json.data}/?spotify`; // SEND URL TO PAGE SOMEHOW OF IP CONNECTED WIFI ????
        } else {
          window.location.href = '/?' + json.redirect;
        }
      } else {
        StatusAlertService.showWarning(
          <div>
            <h3>WiFi Couldnt connect, please check provided credentials</h3>
          </div>,
          {
            autoHide: true,
            withIcon: false,
            withCloseIcon: false,
            removeAllBeforeShow: true,
            autoHideTime: 5000,
          },
        );
      }
    } catch (error) {
      StatusAlertService.showError(
        <div>
          <h3>No response from knob.</h3>
        </div>,
        {
          autoHide: true,
          withIcon: false,
          withCloseIcon: false,
          removeAllBeforeShow: true,
          autoHideTime: 5000,
        },
      );
    }
    setDisabled(false);
  };

  return (
    <div className='comp'>
      <h1>WiFi</h1>
      <form onSubmit={handleSubmit}>
        <label>
          SSID
          <select
            value={manual ? MANUAL_SSID : ssid}
            disabled={scanning}
            onChange={(e) => {
              const value = e.currentTarget.value;
              setManual(value == MANUAL_SSID);
              setSsid(value == MANUAL_SSID ? '' : value);
            }}
          >
            {scanning && <option value=''>Scanning...</option>}
            {networks.map((n) => (
              <option value={n.ssid}>
                {n.ssid} {signalLabel(n.rssi)} {n.secure ? '🔒' : ''}
              </option>
            ))}
            <option value={MANUAL_SSID}>Other network (enter manually)</option>
          </select>
        </label>
        {manual && (
          <label>
            SSID (manual)
            <input
              type='text'
              value={ssid}
              onInput={(e) => setSsid(e.currentTarget.value)}
              required
            />
          </label>
        )}
        <button type='button' onClick={scan} disabled={scanning || disabled}>
          {scanning ? 'Scanning...' : 'Scan again'}
        </button>
        <br />
        <label>
          PASSWORD
          <input
            type='password'
            value={password}
            onInput={(e) => setPswd(e.currentTarget.value)}
            required
          />
        </label>
        <br />
        <button type='submit' disabled={disabled}>
          Submit
        </button>
      </form>
      {status && <p>{status}</p>}
    </div>
  );
};

export default WiFiComp;
