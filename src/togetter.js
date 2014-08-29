/* global removeDiacritics */

var REST_ENDPOINT = "http://to-get.appspot.com/api/";
var CONFIG_URL = "http://dain.se/pebble.html";  // TODO: Move to appspot
var CONFIG_VERSION = 1;

var TYPE_GROUP = "group";
var TYPE_LIST = "list";
var COLLECTED_MASK = 0x80;

var settings = JSON.parse(localStorage.settings || '{"groupId": ""}');
var cache = JSON.parse(localStorage.cache || '{}');
/*
 * cache keys:
 *   current: object // Metadata of current list or group
 *   lastData: string  // JSON string of current list or group
 *   lastMsg: object  // JSON string of last Pebble message
 */

function commitSettings() {
  console.log("Save settings: "+JSON.stringify(settings));
  localStorage.settings = JSON.stringify(settings);
}

function commitCache() {
  console.log("Save cache: "+JSON.stringify(cache));
  localStorage.cache = JSON.stringify(cache);
}

function clearCache() {
  cache = {};
  commitCache();
}

function showList(tries) {
  var msg = cache.lastMsg;
  if(msg) {
    console.log("Send message: "+JSON.stringify(msg));
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

function cleanValue(value) {
  return value.substr(0, 16);
  //return removeDiacritics(value).substr(0, 16);
}

//Get size in bytes of string
function byteLength(str) {
  return encodeURIComponent(str).split(/%(?:u[0-9A-F]{2})?[0-9A-F]{2}|./).length - 1;
}

// Pack items list into binary representation (num_items | struct ListItem* | names)
function pack(items, labelKey, metadata) {
  var data = [items.length];
  var names = "";
  var offset = 0;
  for(var i=0; i<items.length; i++) {
    var item = items[i];
    data = data.concat([offset, metadata(item)]);
    names += cleanValue(item[labelKey]) + '\0';
    offset = byteLength(names);
  }
  data.push(names);
  return data;
}

function msgFromList(list) {
  return {
    label: cleanValue(list.label),
    items: pack(list.items, "item", function(item) { return (item.collected ? COLLECTED_MASK : 0) | item.amount; })
  };
}

function msgFromGroup(group) {
  return {
    label: group.label,
    items: pack(group.lists, "label", function(list) { return 0; })
  };
}

function refreshCurrent() {
  if(cache.current) {
    httpRequest(REST_ENDPOINT+cache.current.path, "GET", function(json) {
      if(cache.lastData != json) {
        cache.lastData = json;
        var data = JSON.parse(json);
        cache.lastMsg = cache.current.type == TYPE_GROUP ? msgFromGroup(data) : msgFromList(data);
        commitCache();
        showList();
      }
    }, function(err_code) {

    });
  }
}

function selectGroup(groupId) {
  cache.current = {
    type: TYPE_GROUP,
    groupId: groupId,
    path: groupId+"/"
  };
  commitCache();
  refreshCurrent();
}

function selectList(groupId, listId) {
  cache.current = {
    type: TYPE_LIST,
    groupId: groupId,
    listId: listId,
    path: groupId+"/lists/"+listId+"/"
  };
  commitCache();
  refreshCurrent();
}

function httpRequest(url, method, success, error) {
  console.log(method+" "+url);
  var req = new XMLHttpRequest();
  req.open(method, url, true);
  req.onload = function(e) {
    if (req.readyState == 4) {
      if(req.status == 200) {
        console.log("Success: "+req.responseText);
        if(success) {
          success(req.responseText);
        }
      } else {
        console.log("Error: "+req.status);
        if(error) {
          error(req.status);
        }
      }
    }
  };

  req.send(null);
}

function updateItem(item) {
  var url = REST_ENDPOINT+cache.current.path;
  url += "?action=update&item="+encodeURIComponent(item.item)+"&collected="+JSON.stringify(item.collected);
  httpRequest(url, "POST", function(resp) {
    console.log("Item updated: "+resp);
  }, function(err_code) {
    
  });
}

/*
 * Event handlers
 */

Pebble.addEventListener("ready", function(e) {
  console.log("connect: " + e.ready);
  showList();
  refreshCurrent();
});

Pebble.addEventListener("appmessage", function(e) {
  console.log("Got message: "+JSON.stringify(e.payload));
  var index = e.payload.select;
  if(index == -1) {
    console.log("Show group");
    selectGroup(settings.groupId);
  } else if(index == -2) {
    refreshCurrent();
  } else {
    console.log("Selected item: "+cache.lastData);
    var data = JSON.parse(cache.lastData);
    if(cache.current.type == TYPE_LIST) {
      console.log("(un)check item");
      var item = data.items[index];
      var collected = !item.collected;
      item.collected = collected;
      cache.lastData = JSON.stringify(data);
      commitCache();
      updateItem(item);
    } else {
      console.log("Show sublist");
      selectList(settings.groupId, data.lists[index].id);
    }
  }
});

// Configuration
Pebble.addEventListener("showConfiguration", function(e) {
  console.log("show configuration");
  settings.version = CONFIG_VERSION;
  Pebble.openURL(CONFIG_URL+"#"+JSON.stringify(settings));
});

Pebble.addEventListener("webviewclosed", function(e) {
  console.log("configuration closed");
  var newSettings = JSON.parse(decodeURIComponent(e.response));
  if(newSettings.groupId != settings.groupId) {
    settings.groupId = newSettings.groupId;
    commitSettings();
    clearCache();
    selectGroup(settings.groupId);
  }
});