/* global removeDiacritics */

var groupId = "YY8h5rM2Z";
var listId = "gkI3qzYzX";

function cleanValue(value) {
  return removeDiacritics(value).substr(0, 16);
}

var chr = String.fromCharCode;

// Pack items list into binary representation (num_items | struct ListItem* | names)
function packItems(items) {
  var struct_arr = "";
  var names = "";
  var offset = 0;
  for(var i=0; i<items.length; i++) {
    var item = items[i];
    struct_arr += chr(offset) + chr(item.amount) + chr(item.collected ? 1 : 0);
    names += cleanValue(item.item) + chr(0);
    offset = names.length;
  }
  return chr(items.length) + struct_arr + names;
}

function updateList() {
  var req = new XMLHttpRequest();
  req.open("GET", "http://to-get.appspot.com/api/"+groupId+"/lists/"+listId+"/", true);
  req.onload = function(e) {
    if (req.readyState == 4 && req.status == 200) {
      if(req.status == 200) {
        var resp = JSON.parse(req.responseText);
        var data = {
          label: cleanValue(resp.label),
          items: packItems(resp.items)
        };
       
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