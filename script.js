var Socket;

function start() {
  document.getElementById("systemMode").value = "Connecting...";
  Socket = new WebSocket("ws://" + window.location.hostname + ":81/");
  Socket.onmessage = function (event) {
    document.getElementById("systemMode").value = "Online";
    var eventDataArr = event.data.split("_");
    if (eventDataArr[0] == "STATUS") {
      document.getElementById("systemMode").value = eventDataArr[1];
      document.getElementById("systemTime").value = eventDataArr[2];  
    } else if (eventDataArr[0] == "CONFIG") {
      document.getElementById("subTimer1Hours").value = eventDataArr[1];
      document.getElementById("subTimer1Mins").value = eventDataArr[2];
      document.getElementById("subTimer2Hours").value = eventDataArr[3];
      document.getElementById("subTimer2Mins").value = eventDataArr[4];
      document.getElementById("sbuTimer1Hours").value = eventDataArr[5];
      document.getElementById("sbuTimer1Mins").value = eventDataArr[6];
      document.getElementById("sbuTimer2Hours").value = eventDataArr[7];
      document.getElementById("sbuTimer2Mins").value = eventDataArr[8];
    }
  };
}

function sub() {
  Socket.send("CMD:SUB");
}

function sbu() {
    Socket.send("CMD:SBU");
}

function updateSUBConfig() {
  var subTimer1Hours = parseInt(document.getElementById("subTimer1Hours").value);
  var subTimer1Mins = parseInt(document.getElementById("subTimer1Mins").value);
  var subTimer2Hours = parseInt(document.getElementById("subTimer2Hours").value);
  var subTimer2Mins = parseInt(document.getElementById("subTimer2Mins").value);
  
  var configData =
    "SUB:" + subTimer1Hours.toString() + ":" + subTimer1Mins.toString() + ":"
    + subTimer2Hours.toString() + ":" + subTimer2Mins.toString() ;
  Socket.send(configData);
  window.alert("Configuration Updated..!");
}

function updateSBUConfig() {
    var sbuTimer1Hours = parseInt(document.getElementById("sbuTimer1Hours").value);
    var sbuTimer1Mins = parseInt(document.getElementById("sbuTimer1Mins").value);
    var sbuTimer2Hours = parseInt(document.getElementById("sbuTimer2Hours").value);
    var sbuTimer2Mins = parseInt(document.getElementById("sbuTimer2Mins").value);
    
    var configData =
      "sBU:" + sbuTimer1Hours.toString() + ":" + sbuTimer1Mins.toString() + ":"
      + sbuTimer2Hours.toString() + ":" + sbuTimer2Mins.toString() ;
    Socket.send(configData);
    window.alert("Configuration Updated..!");
  }
  
