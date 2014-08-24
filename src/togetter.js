/* global removeDiacritics */

var COLLECTED_MASK = 0x80;

var settings = JSON.parse(localStorage.settings || '{"groupId": "", "listId": ""}');
var cache = JSON.parse(localStorage.cache || '{}');

function commitSettings() {
  console.log("Save settings: "+JSON.stringify(settings));
  localStorage.settings = JSON.stringify(settings);
}

function commitCache() {
  console.log("Save cache: "+JSON.stringify(cache));
  localStorage.cache = JSON.stringify(cache);
}

function showList(tries) {
  var msg = JSON.parse(cache.lastMsg || "{}");
  if(msg) {
    console.log("Send message: "+cache.lastMsg);
    Pebble.sendAppMessage(msg, function(e) {
      // Success
    }, function(e) {
      if(tries === undefined) {
        tries = 5;
      }
      if(tries > 0) {
        console.log("Failed sending message, retry...");
        showList(tries--);
      } else {
        console.log("Failed sending message, giving up.");
      }
    });
  } else {
    console.log("No data to show");
  }
}

function updateList(json) {
  var data = JSON.parse(json);
  if(cache.current != json) {
    cache.current = json;
    packList(data);
  }
}

function updateGroup(json) {
  console.log("updateGroup: "+json);
  var data = JSON.parse(json);
  if(cache.current != json) {
    cache.current = json;
    packGroup(data);
  }
}

//var groupId = "YY8h5rM2Z";
//var listId = "gkI3qzYzX";

function cleanValue(value) {
  return removeDiacritics(value).substr(0, 16);
}

// Pack items list into binary representation (num_items | struct ListItem* | names)
function pack(label, items, labelKey, metadata) {
  var oldData = cache.lastMsg || "";
  var data = [items.length];
  var names = "";
  var offset = 0;
  for(var i=0; i<items.length; i++) {
    var item = items[i];
    data = data.concat([offset, metadata(item)]);
    names += cleanValue(item[labelKey]) + '\0';
    offset = names.length;
  }
  data.push(names);
  
  cache.lastMsg = JSON.stringify({
    label: cleanValue(label),
    items: data
  });
  
  console.log("packed: "+cache.lastMsg);
  
  if(oldData != cache.lastMsg) {
    commitCache();
    showList();
  }
}

function packList(list) {
  pack(list.label, list.items, "item", function(item) { return (item.collected ? COLLECTED_MASK : 0) | item.amount; });
}

function packGroup(group) {
  pack(group.label, group.lists, "label", function(list) { return 0; });
}

function httpRequest(url, method, cb) {
  console.log(method+" "+url);
  var req = new XMLHttpRequest();
  req.open(method, url, true);
  req.onload = function(e) {
    if (req.readyState == 4) {
      if(req.status == 200) {
        console.log("Success: "+req.responseText);
        cb(req.responseText);
      } else {
        console.log("Error: "+req.status);
      }
    }
  };

  req.send(null);
}

function updateItem(item) {
  var url ="http://to-get.appspot.com/api/"+settings.groupId+"/lists/"+settings.listId+"/";
  url += "?action=update&item="+item.item+"&collected="+JSON.stringify(item.collected);
  httpRequest(url, "POST", function(resp) {
    console.log("Item updated: "+resp);
  });
}

function refreshList() {
  var url = "http://to-get.appspot.com/api/"+settings.groupId+"/lists/"+settings.listId+"/";
  httpRequest(url, "GET", updateList);
}

function refreshGroup() {
  var url = "http://to-get.appspot.com/api/"+settings.groupId+"/";
  httpRequest(url, "GET", updateGroup);
}

Pebble.addEventListener("ready", function(e) {
  console.log("connect: " + e.ready);
  cache = {};
  commitCache();
  showList();
  if(settings.listId) {
    console.log("refresh list");
    refreshList();
  } else {
    console.log("refresh group");
    refreshGroup();
  }
});

Pebble.addEventListener("appmessage", function(e) {
  console.log("Got message: "+JSON.stringify(e.payload));
  var index = e.payload.select;
  if(index == -1) {
    console.log("Show group");
    settings.listId = "";
    commitSettings();
    refreshGroup();
  } else {
    console.log("Selected item: "+cache.current);
    var data = JSON.parse(cache.current);
    if(settings.listId) {
      console.log("(un)check item");
      var item = data.items[index];
      var collected = !item.collected;
      item.collected = collected;
      cache.current = JSON.stringify(data);
      commitCache();
      updateItem(item);
    } else {
      console.log("Show sublist");
      settings.listId = data.lists[index].id;
      commitSettings();
      refreshList();
    }
  }
});

// Configuration
Pebble.addEventListener("showConfiguration", function(e) {
  console.log("show configuration");
  //TODO: Move page to to-get.appspot.com
  Pebble.openURL("http://dain.se/pebble.html#"+JSON.stringify(settings));
});

Pebble.addEventListener("webviewclosed", function(e) {
  console.log("configuration closed");
  settings = JSON.parse(decodeURIComponent(e.response));
  commitSettings();
  //TODO: Clear cache if groupId changed.
  if(settings.listId) {
    console.log("refresh list");
    refreshList();
  } else {
    console.log("refresh group");
    refreshGroup();
  }
});