
function basename(path){
  var lpath = path.split('/')
  // remove "", e.g for '/' at filename end.
  lpath = lpath.filter(item => item)
  return lpath[lpath.length - 1]
}

function send_ws(command, ...params){

  let api = {
    command: String(command),
    // Note: Syntax 'params[0] || ""', is wrong for param '0'
    param1: (params[0] == undefined?"":String(params[0])),
    param2: (params[1] == undefined?"":String(params[1])),
  }

  // encode params unless fourth argument prevents.
  if (params[2] === undefined || params[2] === true){
    api.param1 = encodeURIComponent(api.param1)
    api.param2 = encodeURIComponent(api.param2)
  }

  DEBUG && console.log(`Sending command: '${api.command}' param1: '${api.param1}' param2: '${api.param2}'`)

  if (ws){
    // TODO: Implement json-parser on server side?!
    // At the moment we send simply send the url scheme and the input is threated like send().
    let uri = `${api.command}/${api.param1}/${api.param2}`
    DEBUG && console.log("Send " + uri)
    ws.send(uri)
    //ws.send(JSON.stringify(api))
  }

}

function send(command, param1, param2, encode){
  if( true ) return send_ws(command, param1, param2, encode)

  param1 = param1 || ""
  param2 = param2 || ""
  encode = encode || true
  if (encode) {
    param1 = encodeURIComponent(param1)

    /* '+' will be threaten like ' ', but filenames could included it. We need to encode it.
     * On the server side, onion's codec.c will decode every %XX char on the fly.
     * Thus a file named "%2F" would be inaccesible if it's not encoded.
     */
    param2 = encodeURIComponent(param2)
  }
  let uri = `api/${api.command}/${api.param1}/${api.param2}`
  DEBUG && console.log(`Sending command: '${command}' param1: '${param1}' param2: '${param2}'`)

  var request = new XMLHttpRequest()
  request.open("post", path)

  request.send(null)
}

function send_url_input(evt) {
  let url_field = document.getElementById("playlistAdd_input")
  let url = url_field.value
  if (url) {
    send("playlist_add", "append", url, false)
    url_field.value = ""
  }
}


function format_time(seconds){
  var date = new Date(null)
  date.setSeconds(seconds)
  return date.toISOString().substr(11, 8)
}

function format_time2(_seconds){
  let sign = ["-","","+"][Math.sign(_seconds)+1]
  let seconds = Math.abs(_seconds)

  if (seconds < 60) {
    return `${sign}${seconds}${UNIT_GAP}s`
  }

  let min =`${sign}${Math.floor(seconds/60)}${UNIT_GAP}min`
  /*if (seconds < 0){
    var min =`${sign}${Math.ceil(seconds/60)}${UNIT_GAP}min`
  }else{
    var min =`${sign}${Math.floor(seconds/60)}${UNIT_GAP}min`
  }*/
  if (0 != seconds % 60){
    min = `${min} ${seconds%60}${UNIT_GAP}s`
  }
  return min
}

function playlist_get_title(entry){
  if (entry.hasOwnProperty('title'))
    return entry['title']

  //Fallback on filename
  return basename(entry['filename'])
}
function check_idle(new_status){
  if ("yes" != (new_status.idle || "")) return false

  if (-1 !== current_playlist_index(new_status.playlist)) return false
  //if (new_status.filename != "undefined") return false // false positive for 'mpv --pause --idle=yes .'

  return true
}


function decode(s){
  /* json was generated by otemplate and contains
   * encoded html chars.
   *
   * Without changing otemplate tool we just
   * can undo this by reverting onion_html_add_enc()
   * (called in otemplate/variables.c by  onion_response_write_html_safe(…) ).
   */

  /*var doc = new DOMParser().parseFromString(s, "text/html");
    return doc.documentElement.textContent;
    */
  // or
  s = s.replaceAll('&#39;', '\'').replaceAll('&quot;','"')
  s = s.replaceAll('&lt;', '<').replaceAll('&gt;', '>')
  s = s.replaceAll('&amp;', '&')
  return s
}


function encodeResolveDifference(s){
  const critical_chars = Array.from("#$&+,/:;=?@")
  let map = {};
  critical_chars.forEach((el) => {map[el] = encodeURIComponent(el)}) // Schmerz…

  function replacer(match, offset, string) {
    //console.log("X" + match + " => " + map[match])
    return map[match]
  }
  const r = RegExp("["+Object.keys(map)+"]", "g")
  console.log(Object.keys(map))
  return s.replace(r, replacer);
}

function encode_raw_link(s){
  // Converts reative and absolute urls in same form.
  // Encodes everything behind the hostname.
  //
  var url = new URL(s, window.location)
  /* Notes: • url.href. is already encoded, but with the 'wrong' variant.
   *        • This approach encodes all slashes!
   */
  //url.pathname = encodeResolveDifference(url.href.slice(1+url.origin.length))
  //or
  url.pathname = encodeURIComponent(decodeURI(url.href.slice(1+url.origin.length)))

  return url.href
}

function preserve_special_chars(s){
  // onion runs decode() on every %XX-string.
  // Encode '%' itsself and then ' ' and '+' should be good enough
  // to get correct filenames on server side after decoding.
  // Well, using encodeURIComponent() should also work.
  return s.replaceAll("%", "%25").replaceAll("+", "%2B").replaceAll(" ", "%20")
  // The should fix error for files with '%', ' ', '+'. 
}

// From simple-mpv-webui
function sanitize(string) {
  // https://stackoverflow.com/a/48226843
  const map = {
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#x27;",
    "/": "&#x2F;",
    "`": "&grave;",
  };
  const reg = /[&<>"'/`]/gi;
  return string.replace(reg, (match) => map[match]);
}
