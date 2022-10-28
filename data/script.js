// Code inspired by: https://randomnerdtutorials.com/esp8266-nodemcu-plot-readings-charts-multiple/

// Get current sensor readings when the page loads
window.addEventListener('load', getReadings);

// Create voltage and current waveform Chart
var chartVW = new Highcharts.Chart({
  chart: {
    renderTo: 'chart-VW',
    marginRight: 80,
	
  },
  series: [{
      name: 'Voltage',
      type: 'line',
      color: '#101D42'
    },
    {
      name: 'Current',
      type: 'line',
      color: '#d9000b',
      yAxis: 1
    },
  ],
  title: {
    text: undefined
  },
  xAxis: {
    labels: {
      format: '{value} ms',
	  style: {
			fontSize: "25px"
		}
    },
    minRange: 5,
    title: {
      text: 'Time, ms',
	  style: {
			fontSize: "25px"
		}
    },
    accessibility: {
      rangeDescription: 'Range: 0 to 42 ms.'
    }
  },
  yAxis: [{
    title: {
      text: 'Voltage, V',
	  style: {
			fontSize: "25px"
		}
    }
  }, {
    opposite: true,
    title: {
      text: 'Current, A',
	  style: {
			fontSize: "25px"
		}
    }
  }],
  credits: {
    enabled: false
  }
});

// Create voltage FFT Chart
var chartVFFT = new Highcharts.Chart({
  chart: {
    renderTo: 'chart-VFFT'
  },
  series: [{
    name: 'Amplitude',
    type: 'line',
    color: '#101D42'/*,
    marker: {
      symbol: 'circle',
      radius: 3,
      fillColor: '#101D42',
    }*/
  }, ],
  title: {
    text: undefined
  },
  xAxis: {
    labels: {
      format: '{value} Hz'
    },
    minRange: 5,
    title: {
      text: 'Frequency, Hz'
    },
    accessibility: {
      rangeDescription: 'Range: 0 to 512 Hz.'
    }
  },
  yAxis: {
    title: {
      text: 'Amplitude, V'
    }
  },
  credits: {
    enabled: false
  }
});

// Create current FFT Chart
var chartIFFT = new Highcharts.Chart({
  chart: {
    renderTo: 'chart-IFFT'
  },
  series: [{
    name: 'Amplitude',
    type: 'line',
    color: '#d9000b'
  }, ],
  title: {
    text: undefined
  },
  xAxis: {
    labels: {
      format: '{value} Hz'
    },
    minRange: 5,
    title: {
      text: 'Frequency, Hz'
    },
    accessibility: {
      rangeDescription: 'Range: 0 to 512 Hz.'
    }
  },
  yAxis: {
    title: {
      text: 'Amplitude, A'
    }
  },
  credits: {
    enabled: false
  }
});

// Create current FFT Chart
var chartTrigger = new Highcharts.Chart({
  chart: {
    renderTo: 'chart-Trigger'
  },
  series: [{
    name: 'Voltage',
    type: 'line',
    color: '#101D42'
  },
  {
    name: 'Current',
    type: 'line',
    color: '#d9000b',
    yAxis: 1
  },
  ],
  title: {
    text: undefined
  },
  xAxis: {
    labels: {
      format: '{value} ms'
    },
    minRange: 5,
    title: {
      text: 'time, ms'
    },
    accessibility: {
      rangeDescription: 'Range: 0 to 40 ms.'
    }
  },
  yAxis: [{
    title: {
      text: 'Voltage, V'
    }
  }, {
    opposite: true,
    title: {
      text: 'Current, A'
    }
  }],
  credits: {
    enabled: false
  }
});

//Plot values in the charts
function plotjson(jsonValue, nr) {

  //Clearing carts for new data, 0 and 1 for waveform chart, 2 for voltage FFT, 3 for current FFT, 4 and 5 for trigger waveform
  if (Number(nr) == 0 || Number(nr) == 1) {
    chartVW.series[nr].setData([]);
  } else if (Number(nr) == 2) {
    chartVFFT.series[0].setData([]);
  } else if (Number(nr) == 3) {
    chartIFFT.series[0].setData([]);
  } else if (Number(nr) == 4) {
    chartTrigger.series[0].setData([]);
  } else if (Number(nr) == 5) {
    chartTrigger.series[1].setData([]);
  }

  var keys = Object.keys(jsonValue);
  console.log(nr);
  //console.log(keys.length);

  for (var i = 0; i < keys.length; i++) {
    const key = keys[i];
    var y = Number(jsonValue[key]);

    if (Number(nr) == 0) {
      chartVW.series[0].addPoint([i * 0.25, y], false, false, true);
    } else if (Number(nr) == 1) {
      chartVW.series[1].addPoint([i * 0.25, y], false, false, true);
    } else if (Number(nr) == 2) {
      chartVFFT.series[0].addPoint([i*1.953125 , y], false, false, true);
    } else if (Number(nr) == 3) {
      chartIFFT.series[0].addPoint([i*1.953125 , y], false, false, true);
    } else if (Number(nr) == 4) {
      chartTrigger.series[0].addPoint([i * 0.25, y], false, false, true);
    } else if (Number(nr) == 5) {
      chartTrigger.series[1].addPoint([i * 0.25, y], false, false, true);
    }

  }

  if (Number(nr) == 0 || Number(nr) == 1) {
    chartVW.redraw();
  } else if (Number(nr) == 2) {
    chartVFFT.redraw();
  } else if (Number(nr) == 3) {
    chartIFFT.redraw();
  } else if (Number(nr) == 4 || Number(nr) == 5 ) {
    chartTrigger.redraw();
  }
  
}

// Function to get current readings to the webpage when it loads for the first time and create table for gathered data 
function getReadings() {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
    }
  };
  xhr.open("GET", "/readings", true);
  xhr.send();
  tableCreate();
}

if (!!window.EventSource) {
  var source = new EventSource('/events');

  source.addEventListener('open', function(e) {
    console.log("Events Connected");
  }, false);

  source.addEventListener('error', function(e) {
    if (e.target.readyState != EventSource.OPEN) {
      console.log("Events Disconnected");
    }
  }, false);

  source.addEventListener('message', function(e) {
    console.log("message", e.data);
  }, false);

  source.addEventListener('new_voltage', function(e) {
    var myObj = JSON.parse(e.data);
    plotjson(myObj, 0);
  }, false);
  source.addEventListener('new_current', function(e) {
    var myObj = JSON.parse(e.data);
    plotjson(myObj, 1);
  }, false);
  source.addEventListener('new_VFFT', function(e) {
    var myObj = JSON.parse(e.data);
    plotjson(myObj, 2);
  }, false);
  source.addEventListener('new_IFFT', function(e) {
    var myObj = JSON.parse(e.data);
    plotjson(myObj, 3);
  }, false);
  source.addEventListener('new_data', function(e) {
    var myObj = JSON.parse(e.data);
    tableUpdate(myObj);
  }, false);
  source.addEventListener('new_trigger_current', function(e) {
    var myObj = JSON.parse(e.data);
    plotjson(myObj, 5);
  }, false);
  source.addEventListener('new_trigger_voltage', function(e) {
    var myObj = JSON.parse(e.data);
    plotjson(myObj, 4);
  }, false);
  source.addEventListener('uptime', function(e) {
    var myObj = parseInt(e.data, 10);
    msToTime(myObj);
  }, false);
}

//Create table for data
function tableCreate() {
  var table = document.getElementById('insertdataTable');
  for (var i = 0; i < 15; i++) {
    var x = document.getElementById('insertdataTable').insertRow(i);
    for (var c = 0; c < 2; c++) {
      var z = x.insertCell(c);
    }
    table.rows[i].cells[0].innerHTML = i;
    table.rows[i].cells[1].innerHTML = i;
  }
}

//Update table values
function tableUpdate(jsonValue) {
  var table = document.getElementById('insertdataTable');
  var keys = Object.keys(jsonValue);
  for (var i = 0; i < 15; i++) {
    const key = keys[i];
    var y = Number(jsonValue[key]);
    table.rows[i].cells[0].innerHTML = key;
    table.rows[i].cells[1].innerHTML = y;
  }
}

//Send button command to MCU
function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked)
  { 
    xhr.open("GET", "/update?output="+element.id+"&state=1", true); 
  }
  else 
  { 
    xhr.open("GET", "/update?output="+element.id+"&state=0", true); 
  }
  xhr.send();
}

//Send waveform capture request
function sendTriggerRequest () {
  var xhr = new XMLHttpRequest();
  const rbs = document.querySelectorAll('input[name="choice"]');
  let selectedValue;
  for (const rb of rbs) {
    if (rb.checked) {
      selectedValue = rb.value;
      break;
    }
  }
  var showData1= document.getElementById("nr").value;
  var showData2= document.getElementById("tlevel").value;
  var showData3= document.getElementById("timeout").value;

  //alert ("Request sent, waiting for reply from device");
  xhr.open("GET", "/update?count="+showData1+"&level=" + showData2 +"&timeout=" + showData3  + "&select=" + selectedValue, true); 
  xhr.send();
}

//Convert ms to seconds, minutes, hours and days
function msToTime(s) {
  var secs = s % 60;
  s = (s - secs) / 60;
  
  var mins = s % 60;
  s = (s - mins) / 60;
    
  var hrs = (s) % 24;
  s = (s - hrs) / 24;
  
  var days = s;
  document.getElementById("uptime").textContent = "Device uptime "+ days + " days, " + hrs + " hours, " + mins + " minutes, " + secs + " seconds";
}

//Send calibration request
function sendCalibRequest() {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      console.log(this.responseText);
    }
  };
  xhr.open("GET", "/calib", true);
  xhr.send();
}