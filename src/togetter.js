var groupId = "YY8h5rM2Z";
var listId = "StsNkSvIa";

function updateList() {
  var req = new XMLHttpRequest();
  req.open("GET", "http://to-get.appspot.com/api/"+groupId+"/lists/"+listId+"/", true);
  req.onload = function(e) {
    if (req.readyState == 4 && req.status == 200) {
      if(req.status == 200) {
        var resp = JSON.parse(req.responseText);
        var data = {
          label: removeDiacritics(resp.label),
          items: String.fromCharCode(resp.items.length)
        };
        
        for(var i=0; i<resp.items.length; i++) {
          var item = resp.items[i];
          data.items += String.fromCharCode(item.collected ? 1 : 0) +
            String.fromCharCode(item.amount) +
            String.fromCharCode(item.item.length) +
            removeDiacritics(item.item);
        }
        
        console.log(JSON.stringify(data));
        Pebble.sendAppMessage(data);
      } else {
        console.log("Error");
      }
    }
  };

  req.send(null);
}

Pebble.addEventListener("ready", function(e) {
  console.log("connect!" + e.ready);
  updateList();
});