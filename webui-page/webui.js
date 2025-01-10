var DEBUG = false,
    DEBUG_MOBILE = false,
    use_dummy_audio_element = false, /* for controls on lock screen. */
    metadata = {},
    subs = {},
    audios = {},
    videos = {},
    connected = false,
    max_title_size = 60,
    tab_in_background = false

var ws = null
var mpv_status = {}

var block = {
      posSlider: null, // null, false or Timeout-ID during user interaction
      volSlider: null,

      active: function (name){
        return (this[name] !== null)  // Returns even true for 'false'!
      },
      enable: function (name){
        if (this[name]) { // Remove running timeout
          clearTimeout(this[name])
        }
        this[name] = false
      },
      disable: function (name){ // Resets value after timeout
        if (this[name] === false) {
          this[name] = setTimeout(function(){
            window.block[name] = null // Not 'this[name]' !
          }, this.timeout_ms)
        }
      },
      timeout_ms: 300,
      doublePause: false, // flag used without above timeout mechanism
    }

const REGEXS = {
  'brackets': new RegExp('\\[[^\\]]*\\]', 'g'), // [...]
  'extension': new RegExp('[.][A-z]{3,10}$', ''),   // .ext
  'checksumBeforeExtension': new RegExp('-[A-z0-9-]{10,12}([.][A-z]+)$', ''), // -AbCdEXFGL.
  'cleanup_dots': new RegExp('[.…]*…[.…]*','g'),
}

/* Collect [num] milliseconds status updates before
 * they will be used to update the page. This is useful to
 * avoid too many updates for fast changing variables.
 */
var mpv_outstanding_status = {
  delay: 300,
  updates: {},
  timer: null,
  pending: false
}

function send_ws(command, ...params){

  api = {
    command: command,
    // Note: Syntax 'params[0] || ""', is wrong for param '0'
    param1: (params[0] == undefined?"":params[0]),
    param2: (params[1] == undefined?"":params[1]),
  }

  DEBUG && console.log(`Sending command: '${api.command}' param1: '${api.param1}' param2: '${api.param2}'`)

  if (ws){
    // TODO: Implement json-parser on server side?!
    // At the moment we send simply send the url scheme
    uri = String(api.command) + "/" + String(api.param1) + "/" + String(api.param2)
    DEBUG && console.log("Send " + uri)
    ws.send(uri)
    //ws.send(JSON.stringify(api))
  }

}

// ===================================
/* On MDN
 * https://developer.mozilla.org/en-US/docs/Web/API/window/requestAnimationFrame
 * is written:
 *     'requestAnimationFrame() calls are paused in most browsers
 *     when running in background tabs'
 *
 * but it turn out that just the intervall is reduced. So we
 * need to build this by hand.
 */
function checkForStatusUpdate(immedialty){
  if (immedialty && !window.connected && !window.tab_in_background){
    clearTimeout(window.mpv_outstanding_status.restart)
    status_init_ws()
    return
  }
  if (!window.mpv_outstanding_status.pending) return;

  var _handler = function() {
    if (window.tab_in_background) {
      //console.log('Wait at ' + Date.now());
      window.mpv_outstanding_status.timer = setTimeout(
        _handler, window.mpv_outstanding_status.delay)
      return
    }

    window.mpv_outstanding_status.timer = null

    //TODO Prepare async handling by storing update in local var (!?)
    var updates = window.mpv_outstanding_status.updates
    window.mpv_outstanding_status.updates = {}
    //console.log('Handle update at ' + Date.now());
    handleStatusUpdate(updates)
  }

  if (immedialty){
    clearTimeout(window.mpv_outstanding_status.timer)
    _handler()
  }else if (window.mpv_outstanding_status.delay == 0 ){
    _handler()
  }else{
    if (window.mpv_outstanding_status.timer == null ){
      window.mpv_outstanding_status.timer = setTimeout(
        _handler, window.mpv_outstanding_status.delay)
    }
  }

}
document.addEventListener('visibilitychange', function (event) {
  if (document.hidden) {
    tab_in_background = true
  } else {
    tab_in_background = false
    checkForStatusUpdate(true)
  }
});
// ===================================



function basename(path){
  var lpath = path.split('/')
  // remove "", e.g for '/' at filename end.
  lpath = lpath.filter(item => item)
  return lpath[lpath.length - 1]
}

function send(command, param1, param2){
  if( true ) return send_ws(command, param1, param2)

  DEBUG && console.log(`Sending command: '${command}' param1: '${param1}' param2: '${param2}'`)
  var path = 'api/' + command
  if (param1 !== undefined)
    path += "/" + param1
  if (param2 !== undefined)
    path += "/" + param2

  var request = new XMLHttpRequest();
  request.open("post", path)

  request.send(null)
}

function togglePlaylist() {
  document.body.classList.toggle('noscroll')
  var el = document.getElementById("overlay")
  el.style.visibility = (el.style.visibility === "visible") ? "hidden" : "visible"
}

function createPlaylistTable(entry, position, pause, first) {
  function setActive(set) {
    if (set === true) {
      td_left.innerHTML = '<i class="fas fa-play"></i>'
      td_left.classList.add('active')
      td_2.classList.add('active')
    } else {
      td_left.replaceChildren()
      td_left.classList.remove('active')
      td_2.classList.remove('active')
    }
  }

  function blink() {
     td_left.classList.add('click')
     td_2.classList.add('click')
     setTimeout(function(){ td_left.classList.remove('click')
     td_2.classList.remove('click');}, 100)
  }

  if (entry.title) {
    var title = entry.title
  } else {
    var title = basename(entry.filename)
    // Parts of URLs needs decoding here.
    title = decodeURI(title)
  }

  // limit length of entry
  var title_trimmed = trim_title_string(title, max_title_size)

  var table = document.createElement('table')
  var tr = document.createElement('tr')
  var td_left = document.createElement('td')
  var td_2 = document.createElement('td')
  var td_right = document.createElement('td')
  table.className = 'playlist'
  table.playlist_id = position // Read by some function handler
  tr.className = 'playlist'
  td_2.className = 'playlist'
  td_left.className = 'playlist'
  td_right.className = 'playlist'
  td_2.innerText = title_trimmed
  td_2.setAttribute('title', title)

  var td_3 = document.createElement('td')
  td_3.innerHTML = '<i class="fas fa-arrow-up"></i>'
  td_3.className = 'playlist'

  td_right.innerHTML = '<i class="fas fa-trash"></i>'

  if (entry.hasOwnProperty('playing')) {
    if (pause === "yes") {
      td_left.innerHTML = '<i class="fas fa-pause"></i>'
    } else {
      td_left.innerHTML = '<i class="fas fa-play"></i>'
    }

    td_left.classList.add('playing')
    td_left.classList.add('violet')
    td_2.classList.add('playing')
    td_2.classList.add('violet')
    td_3.classList.add('violet')
    td_right.classList.add('violet')

  } else {
    td_left.classList.add('gray')
    td_2.classList.add('gray')
    td_3.classList.add('gray')
    td_right.classList.add('gray')

    function playlist_jump(table) {
      arg = table.playlist_id
      send("playlist_jump", arg)
      send("play");  // remove pause
      // evt.stopPropagation();
    }

    td_left.addEventListener('click', () => { playlist_jump(table) })
    //td_2.addEventListener('click', () => { playlist_jump(table) })

    td_left.addEventListener("mouseover", function() {setActive(true)})
    td_left.addEventListener("mouseout", function() {setActive(false)})
    //td_2.addEventListener("mouseover", function() {setActive(true)})
    //td_2.addEventListener("mouseout", function() {setActive(false)})

    td_left.addEventListener("click", blink)
    //td_2.addEventListener("click", blink)
  }

  td_3.addEventListener('click',
    function (table) {
      return function () {
        arg = table.playlist_id
        send("playlist_move_up", arg)
      }
    }(table))

  td_right.addEventListener('click',
    function(table) {
      return function() {
        arg = table.playlist_id
        send("playlist_remove", arg)
      }
    }(table)
  )

  tr.appendChild(td_left)
  tr.appendChild(td_2)
  tr.appendChild(td_3)
  tr.appendChild(td_right)
  table.appendChild(tr)
  return table
}

function populatePlaylist(json, pause) {
  var playlist = document.getElementById('playlist')
  playlist.replaceChildren()

  var first = true
  var playedRow = null
  for(var i = 0; i < json.length; ++i) {
    row = createPlaylistTable(json[i], i, pause, first);
    playlist.appendChild(row)
    if (first === true) {
      first = false
    }

    if (json[i].hasOwnProperty('playing')) {
      playedRow = row;
    }
  }

  // Scroll played entry into view
  if (playedRow) playedRow.scrollIntoView({block: 'center'});

}

function updatePlaylistOnPause(current_playlist, new_pause, old_pause) {
  if (new_pause == old_pause) return;
  var playlist = document.getElementById('playlist')

  for (i=0; i<current_playlist.length; ++i){
    if (current_playlist[i].current && current_playlist[i].current == true){

      row = createPlaylistTable(current_playlist[i], i, new_pause, (i==0));
      playlist.replaceChild(row, playlist.children[i])
      break;
    }
  }
}

function updatePlaylist(new_playlist, old_playlist, new_pause) {
  /* Compare both lists and just update changed rows.
   * It works well if just one element is removed/inserted
   * or some Elements are swapped.
   *
   * Inserts of multiple elements can not be detected and
   * will handled like a complete new (part of a) list.
   */

  var compare_handler = function(new_entry, old_entry){
    if (new_entry.filename == old_entry.filename
      && (new_entry.current == old_entry.current))
      return true;
    return false
  }

  var operations = get_diff(new_playlist, old_playlist, compare_handler)
  //DEBUG && console.log(operations)

  var playlist = document.getElementById('playlist')
  // children Index of playlist matches with above index.

  var offset = 0
  operations.forEach( x => {
    if (x['op'] == 'replace' /* || x['op'] == 'changed'*/) {
      var row = createPlaylistTable(
        new_playlist[x['new']],
        x['old'], new_pause, (x['new']==0));
      playlist.replaceChild( row,
        playlist.children[x['old'] + offset])
    }else if (x['op'] == 'add') {
      var row = createPlaylistTable(
        new_playlist[x['new']],
        x['old'], new_pause, (x['new']==0));
      playlist.insertBefore( row,
        playlist.children[x['old'] + offset])//.nextElementSilbing)
      offset++
    }else if (x['op'] == 'remove') {
      playlist.removeChild(
        playlist.children[x['old'] + offset])
      offset--
    }
  })

  // Update playlist indizes for event handlers.
  var id = 0;
  playlist.childNodes.forEach(n => { n.playlist_id = id++; })

  /* Scroll played entry into view, if menu is hidden.
   * Scrolling if open would be anoying...
   * Maybe a button to scroll to active entry would be nice
   * to catch the open playlist case.
   */
  if (overlay.style.getPropertyValue("visibility") == "hidden"){
    var pl_index = current_playlist_index(new_playlist)
    if (pl_index) {
      playlist.children[pl_index].scrollIntoView({block: 'center'})
    }
  }

}

// TODO: Is it possible to use this as static function of KeyBindings class?!
function unfold_bindings(bindings){
  // 1. Split into two sub-arrays by the shift-flag:
  // The resulting object has the keys "true" and "false"…
  const split_by_modifier_shift = bindings.reduce((result, obj) => {
    const key = (obj.shift?true:false) // merges false and undefined
    result[key] = result[key] || []
    result[key].push(obj);
    return result;
  }, {});

  // For both sub-lists, create map for key code and key name.
  // => [shift][keyname] or [shift][keycode] can be used to access available keybindigs.
  return {
    false: Object.assign({},
      Object.groupBy(split_by_modifier_shift[false], (b) => b.code ),
      Object.groupBy(split_by_modifier_shift[false], (b) => b.key ),
    ),
    true: Object.assign({},
      Object.groupBy(split_by_modifier_shift[true], (b) => b.code ),
      Object.groupBy(split_by_modifier_shift[true], (b) => b.key ),
    ),
  }
}

class KeyBindings {
  /* This class will construct two static maps, one for keyCodes and one for keyNames.
   * The constructor takes an Event-object and returns the matching binding or null
   */
  static bindings = [
    {
      "key": " ",
      "code": 32,
      "command": "toggle_pause"
    },
    {
      "key": "ArrowRight",
      "code": 39,
      "command": "seek",
      "param1": "5"
    },
    {
      "key": "ArrowLeft",
      "code": 37,
      "command": "seek",
      "param1": "-5"
    },
    {
      "key": "ArrowUp",
      "code": 38,
      "command": "seek",
      "param1": "60"
    },
    {
      "key": "ArrowDown",
      "code": 40,
      "command": "seek",
      "param1": "-60"
    },
    {
      "key": "ArrowRight",
      "code": 39,
      "shift": true,
      "command": "seek",
      "param1": "1"
    },
    {
      "key": "ArrowLeft",
      "code": 37,
      "shift": true,
      "command": "seek",
      "param1": "-1"
    },
    {
      "key": "ArrowUp",
      "code": 38,
      "shift": true,
      "command": "seek",
      "param1": "5"
    },
    {
      "key": "ArrowDown",
      "code": 40,
      "shift": true,
      "command": "seek",
      "param1": "-5"
    },
    {
      "key": "PageDown",
      "code": 34,
      "command": "seek",
      "param1": "3"
    },
    {
      "key": "PageUp",
      "code": 33,
      "command": "seek",
      "param1": "-3"
    },
    {
      "key": "f",
      "code": 70,
      "command": "fullscreen",
    },
    {
      "key": "n",
      "code": 78,
      "command": "playlist_next",
    },
    {
      "key": "p",
      "code": 80,
      "command": "playlist_prev",
    },
    {
      "key": "Backspace",
      "code": 8,
      "command": "reset_playback_speed",
    },
    {
      "key": "9",
      "code": 57,
      "command": "add_volume",
      "param1": "-2"
    },
    {
      "key": "0",
      "code": 48,
      "command": "add_volume",
      "param1": "2"
    },
  ]

  static bindings2 = unfold_bindings(this.bindings)

  static for_event(evt) {
    const binding = this.bindings2[evt.shiftKey][evt.keyCode] || this.bindings2[evt.shiftKey][evt.key]
    if (binding === undefined) return null
    // Due the construction by groupBy(...) 'binding' is an one-element array. (or multiple if key is used multiple times…)
    return Object.assign(new KeyBindings(), binding[0])
  }
}

function webui_keydown(evt) {

  // We have no shortcuts below that use these combos, so don't capture them.
  // We allow Shift key as some keyboards require that to trigger the keys.
  // For example, a US QWERTY uses Shift+/ to get ?.
  // Additionally, we want to ignore any keystrokes if an input element is focussed.
  if ( evt.altKey || evt.ctrlKey || evt.metaKey) {
    return;
  }
  DEBUG && console.log("Key pressed: " + evt.keyCode + " " + evt.key )
  DEBUG && console.log(evt)

  binding = KeyBindings.for_event(evt) // null or Object assigned to this key+modifier combination.
  if (binding === null) return

  if (document.activeElement.localName === "input") /* localName == tagName.toLowerCase() */
  {
    if (document.activeElement.type !== "range"){
      return // Avoid double interpreation of keystroke in normal input fields
    }
    else { // range slider handling
      // Abort if it's a known key for the slider (= ArrowKeys,?)
      //if (evt.keyCode in [37, 38, 39 ,40]) return // Javascript fooling around ……… :facepalm:
      if ([37, 38, 39, 40].includes(evt.keyCode)) return
    }
  }

  send(binding.command, binding.param1, binding.param2)
  evt.stopPropagation()
  evt.preventDefault()  // e.g. avoids 'scrolling by space key'
}

function format_time(seconds){
  var date = new Date(null);
  date.setSeconds(seconds)
  return date.toISOString().substr(11, 8)
}

function format_time2(seconds){
  if (Math.abs(seconds) < 60) {
    return `${seconds}s`
  }
  if (seconds < 0){
    var min =`${Math.ceil(seconds/60)}min`
  }else{
    var min =`${Math.floor(seconds/60)}min`
  }
  if (0 == seconds % 60){
    var sec = ''
  }else{
    var sec = `${Math.abs(seconds)%60}s`
  }
  return min+sec
}

function setFullscreenButton(fullscreen) {
    if (fullscreen === 'yes') {
        var fullscreenText = 'Fullscreen off'
    } else {
        fullscreenText = 'Fullscreen on'
    }
    document.getElementById("fullscreenButton").innerText =
        fullscreenText
}

function setTrackList(tracklist) {
  window.audios.selected = 0
  window.audios.count = 0
  window.subs.selected = 0
  window.subs.count = 0
  window.videos.selected = 0
  window.videos.count = 0
  for (var i = 0; i < tracklist.length; i++){
    if (tracklist[i].type === 'audio') {
      window.audios.count++
      if (tracklist[i].selected) {
        window.audios.selected = tracklist[i].id
      }
    } else if (tracklist[i].type === 'sub') {
      window.subs.count++
      if (tracklist[i].selected) {
        window.subs.selected = tracklist[i].id
      }
    } else if (tracklist[i].type === 'video') {
      window.videos.count++
      if (tracklist[i].selected) {
        window.videos.selected = tracklist[i].id
      }
    }
  }

  // Subtitle
  var el = document.getElementById("nextSub")
  if (window.subs.count > 0) {
    el.innerText = 'Sub ' + window.subs.selected + '/' + window.subs.count
    displayElementClass('subtitle', true)
  }else{
    displayElementClass('subtitle', false)
    el.innerText = 'No subtitle'
  }

  // Video streams
  var el = document.getElementById("nextVideo")
  if (window.videos.count > 1) {
    el.innerText = 'Video ' + window.videos.selected + '/' + window.videos.count
    displayElementClass('video', true)
  }else{
    displayElementClass('video', false)
    if (window.videos.count == 0) {
      el.innerText = 'No video'
    }
  }

  // Audio tracks
  var el = document.getElementById("nextAudio")
  if (window.audios.count > 1){
    el.innerText = 'Audio ' + window.audios.selected + '/' + window.audios.count
    displayElementClass('audio2', true) // for #audio tracks > 1
    displayElementClass('audio1', true) // for #audio tracks > 0
  }else{
    displayElementClass('audio2', false)
    if (window.audios.count == 1){
      displayElementClass('audio1', true)
      el.innerText = 'Audio ' + window.audios.selected + '/' + window.audios.count
    }else{
      displayElementClass('audio1', false)
      el.innerText = 'No audio'
    }
  }
}

function current_chapter_index(mpv_status){
  if (mpv_status.hasOwnProperty('chapter')){
    return mpv_status['chapter']
  }
  return -1
}

function chapter_get_title(chapters, index){
  if (chapters[index].hasOwnProperty('title'))
    return chapters[index]['title']

  //Fallback
  return "Chapter " + index
}


function current_playlist_index(playlist){
  for (var i = 0; i < playlist.length; i++){
    if (playlist[i].hasOwnProperty('current' /* or 'playing' ?!*/)) {
      return i
    }
  }
  DEBUG && console.log("Can not detect playlist entry!")
}

function playlist_get_title(entry){
  if (entry.hasOwnProperty('title'))
    return entry['title']

  //Fallback on filename
  return basename(entry['filename'])
}

function setMetadata(metadata, playlist, filename) {
  // try to gather the track number
  metadata = metadata?metadata:{}; // null to {}
  var track = ''
  if (metadata['track']) {
    track = metadata['track'] + ' - '
  }

  // try to gather the playing playlist element
  var pl_index = current_playlist_index(playlist)

  // set the title. Try values in this order:
  // 1. title set in playlist
  // 2. metadata['title']
  // 3. metadata['TITLE']
  // 4. filename
  if (pl_index && playlist[pl_index].title) {
    var title = playlist[pl_index].title
  } else if (metadata['title']) {
    var title = track + metadata['title']
  } else if (metadata['TITLE']) {
    var title = track + metadata['TITLE']
  } else {
    var title = track + basename(filename)
  }
  window.metadata.title = trim_title_string(title, max_title_size)

  // set the artist
  if (metadata['artist']) {
    window.metadata.artist = metadata['artist']
  } else {
    window.metadata.artist = ''
  }

  // set the album
  if (metadata['album']) {
    window.metadata.album = metadata['album']
  } else {
    window.metadata.album = ''
  }

  document.getElementById("title").innerText = window.metadata.title
  document.getElementById("title").setAttribute('title', title) // may be longer string

  document.getElementById("artist").innerText = window.metadata.artist
  document.getElementById("album").innerText = window.metadata.album
}

function setPosSlider(position, duration, old_position, old_duration) {
  var slider = document.getElementById("mediaPosition")
  var pos = document.getElementById("position")
  slider.max = duration
  if (!window.block.active('posSlider')) {
    slider.value = position

    if (Math.round(position) != Math.round(old_position)){
      pos.innerText = format_time(slider.value)
    }
  }
}

function setVolumeSlider(volume, volumeMax) {
  var slider = document.getElementById("mediaVolume")
  var vol = document.getElementById("volume")
  if (!window.block.active('volSlider')) {
    slider.value = volume
    slider.max = volumeMax
  }
  vol.innerText = slider.value + "%"
}

function setPlayPause(value) {
  if (value === null) {
    // fallback for webui-onion.bin with incomplete status
    value = (document.getElementById("audio").paused?"yes":"yes")
  }

  updatePlayPauseButton(value)

  if ('mediaSession' in navigator) {
    if (value == "yes") {
      audioPause()
    } else {
      audioPlay()
    }
  }
}

function updatePlayPauseButton(value) {
  var playPause = document.getElementsByClassName('playPauseButton')
  // var playPause = document.getElementById("playPause")

  if (value == "yes") {
    [].slice.call(playPause).forEach(function (div) {
      div.innerHTML = '<i class="fas fa-play"></i>'
    })
  } else {
    [].slice.call(playPause).forEach(function (div) {
      div.innerHTML = '<i class="fas fa-pause"></i>'
    })
  }
}

function __mediaSession() {
  if ('mediaSession' in navigator) {
    navigator.mediaSession.metadata = new MediaMetadata({
      title: window.metadata.title,
      artist: window.metadata.artist,
      album: window.metadata.album,
      artwork: [
        { src: '/favicons/android-chrome-192x192.png', sizes: '192x192', type: 'image/png' },
        { src: '/favicons/android-chrome-512x512.png', sizes: '512x512', type: 'image/png' }
      ]
    });

    navigator.mediaSession.setActionHandler('play', function() {send('play');})
    navigator.mediaSession.setActionHandler('pause', function() {send('pause');})
    navigator.mediaSession.setActionHandler('seekbackward', function() {send('seek', '-5');})
    navigator.mediaSession.setActionHandler('seekforward', function() {send('seek', '10');})
    navigator.mediaSession.setActionHandler('previoustrack', function() {send('playlist_prev');})
    navigator.mediaSession.setActionHandler('nexttrack', function() {send('playlist_next');})

    navigator.mediaSession.setPositionState({
      duration: mpv_status["duration"],
      position: Math.max(0, Math.min(mpv_status["time-pos"], mpv_status["duration"])),
      playbackRate: mpv_status["speed"] //1.0
    })

    if ("pause" in mpv_status && mpv_status["pause"] == "yes" ){
      navigator.mediaSession.playbackState = "paused"
    }else{
      navigator.mediaSession.playbackState = "playing"
    }
  }
}

function togglePlayPause() {
  var pause = mpv_status["pause"]
  //var audio = document.getElementById("audio")

  if (pause === null) {
    // fallback for webui-onion.bin with incomplete status
    pause = (document.getElementById("audio").paused?"yes":"no")
  }

  if (pause == "yes"){
    audioPlay()
  }else{
    audioPause()
  }

  /* Removed because audioPlay/Pause now trigger this already,
   * see audioLoad(). */
  send('toggle_pause')
}

function playlist_loop_cycle() {
  var loopButton = document.getElementsByClassName('playlistLoopButton')[0]
  var value = loopButton.getAttribute("value")
  if (value === "no") {
    send("loop_file", "inf")
    //send("loop_file", "yes")
    send("loop_playlist", "no")
  } else if (value === "1") {
    send("loop_file", "no")
    send("loop_playlist", "inf")
    //send("loop_playlist", "yes")
  } else if (value === "a") {
    send("loop_file", "no")
    send("loop_playlist", "no")
  } else {
    DEBUG && console.log("loopButton has unexpected value '" + value + "'.")
  }
}

function setChapter(num_chapters, chapter, metadata) {
  var chapterContent = document.getElementById('chapterContent')
  var chapter_title = ''
  if (metadata !== null ){
    if (metadata['title']) {
      chapter_title = metadata['title']
    } else if (metadata['TITLE']) {
      chapter_title = metadata['TITLE']
    } else {
      //chapter_title = "No title"
    }
  }
  if (num_chapters === 0) {
    displayElementClass('chapter', false)
    displayElementClass('no_chapter', true)
    chapterContent.innerText = "0/0"
  } else {
    displayElementClass('chapter', true)
    displayElementClass('no_chapter', false)
    chapterContent.innerText = "" + (chapter + 1) + "/" + num_chapters
      + " " + chapter_title
  }
}

function updateCapterMarks(num_chapters, chapter, metadata, chapter_list, duration) {
  /* Enrich the position slider by the chapter marks. Currently
   * this is realized by drawing the marks into a SVG background-image.
   */
  var chapter_marker = document.getElementById('chapter_marker')
  if (num_chapters === 0) {
    chapter_marker.style.backgroundImage = 'none'
  }else{
    if( chapter == -1 ){
      /* If mpv is paused and the played file is changed, duration of the previous file is
       * still the active. We need to wait until chapter >= 0 to guarantee that the correct
       * time was extracted from the file. (*4)
       *
       */
      return;
    }

    var svg_chapter_list = []
    for (var i = 0; i < chapter_list.length; i++){
      const pos_promille = (1000 * chapter_list[i]['time']/duration).toFixed(2)
      svg_chapter_list.push(`<use xlink:href="%23markB" x="${pos_promille}" width="50" height="100%" />\\`)
    }
    chapter_marker.style.backgroundImage = `url('data:image/svg+xml,\\
<svg version="1.1" \\
xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" \\
width="1100" height="auto" preserveAspectRatio="none">\\
<defs>\\
<symbol id="markB" \\
viewBox="0 0 50 200" \\
preserveAspectRatio="xMidYMid" \\
>\\
<polygon \\
points="24,49 0,0 49,0, 25,49, 25,150, 49,199, 0,199, 24,150" \\
fill="rgba(200,30,30, 0.3)" \\
stroke="%23555544" stroke-width="1" vector-effect="non-scaling-stroke"\\
/>\\
</symbol>\\
</defs>\\
<g transform="translate(25 0)">\\
    ${svg_chapter_list.join('\n')}
</g>\\
</svg>')`
  }
}

function setPlaybackSpeedButtons(speed){
  if (window.metadata['speed'] == speed) return;

  window.metadata['speed'] = speed;
  var resetButton = document.getElementById('playbackSpeedReset')
  if (speed && speed != 1.0) {
    showInfoForControls(resetButton, ['playbackSpeed1', 'playbackSpeed2'] , true)
  }else{
    showInfoForControls(resetButton, ['playbackSpeed1', 'playbackSpeed2'] , false)
  }
}

function setSubDelay(fDelay) {
  var subContent = document.getElementById('subDelayInfo')
  if ( fDelay == 0 ){
    showInfoForControls(subContent, ['subDelay1', 'subDelay2'] , false)
  }else{
    const iDelay = parseInt(1000*fDelay)
    subContent.innerText = (iDelay>0?'+':'') + iDelay + 'ms';
    showInfoForControls(subContent, ['subDelay1', 'subDelay2'] , true)
  }
}

function setAudioDelay(fDelay) {
  var audioContent = document.getElementById('audioDelayInfo')
  if ( fDelay == 0 ){
    showInfoForControls(audioContent, ['audioDelay1', 'audioDelay2'] , false)
  }else{
    const iDelay = parseInt(1000*fDelay)
    audioContent.innerText = (iDelay>0?'+':'') + iDelay + 'ms';
    showInfoForControls(audioContent, ['audioDelay1', 'audioDelay2'] , true)
  }
}

function setLoop(loopFile, loopPlaylist) {
  var loopButton = document.getElementsByClassName('playlistLoopButton')
  var html = '<i class="fas fa-redo-alt"></i>'
  var value = ""
  if (loopFile === "no") {
    if (loopPlaylist === "no") {
      html = '!<i class="fas fa-redo-alt"></i>'
      value = "no"
    } else {
      html = '<i class="fas fa-redo-alt"></i>Σ'
      value = "a"
    }
  } else {
    html = '<i class="fas fa-redo-alt"></i>1'
    value = "1"
  }

  [].slice.call(loopButton).forEach(function (div) {
    div.innerHTML = html
    div.setAttribute("value", value)
    })

}

function handleStatusResponse(json) {
  setMetadata(json['metadata'], json['playlist'], json['filename'])
  setTrackList(json['track-list'])
  document.getElementById("duration").innerText =
    format_time(json['duration'])

  document.getElementById("playtime-remaining").innerText =
    "-" + format_time(json['playtime-remaining'])

  setPlaybackSpeedButtons(json['speed'])
  if (json['speed']) {
    document.getElementById("speed").innerText =
      (json['speed'] != 1.0)?`(x ${Number(json['speed']).toFixed(2)})`:''
  }

  setSubDelay(json['sub-delay'])
  setAudioDelay(json['audio-delay'])
  setPlayPause(json['pause'])
  setPosSlider(json['time-pos'], json['duration'], -1, -1)
  setVolumeSlider(json['volume'], json['volume-max'])
  setLoop(json["loop-file"], json["loop-playlist"])
  setFullscreenButton(json['fullscreen'])
  setChapter(json['chapters'], json['chapter'], json['chapter-metadata'])
  populatePlaylist(json['playlist'], json['pause'])
  if ('mediaSession' in navigator) {
    setupNotification()
  }

  updateCapterMarks(json['chapters'], json['chapter'], json['chapter-metadata'],
    json['chapter-list'], json['duration'])
}

function handleStatusUpdate(status_updates) {
  new_status = Object.assign({}, mpv_status, status_updates)
  mpv_outstanding_status.pending = false
  if ("metadata" in status_updates
    ||"playlist" in status_updates
    ||"filename" in status_updates){
    setMetadata(new_status['metadata'], new_status['playlist'], new_status['filename'])
  }
  if ("track-list" in status_updates){
    setTrackList(new_status['track-list'])
  }
  if ("duration" in status_updates){
    document.getElementById("duration").innerText =
      format_time(new_status['duration'])
  }
  if ("speed" in status_updates){
    setPlaybackSpeedButtons(new_status['speed'])

    if( new_status['speed'] != mpv_status['speed']){
      document.getElementById("speed").innerText =
        (new_status['speed'] != 1.0)?`(x ${Number(new_status['speed']).toFixed(2)})`:''
    }
  }
  if ("playtime-remaining" in status_updates){
    if (Math.round(new_status["playtime-remaining"]) != Math.round(mpv_status["playtime-remaining"])){
      document.getElementById("playtime-remaining").innerText =
        "-" + format_time(new_status['playtime-remaining'])
    }
  }
  if ("sub-delay" in status_updates){
    setSubDelay(new_status['sub-delay'])
  }
  if ("audio-delay" in status_updates){
    setAudioDelay(new_status['audio-delay'])
  }
  if ("time-pos" in status_updates
    ||"duration" in status_updates){
    setPosSlider(new_status['time-pos'], new_status['duration'],
      mpv_status['time-pos'], mpv_status['duration'])
  }
  if ("volume" in status_updates
    ||"volume-max" in status_updates){
    setVolumeSlider(new_status['volume'], new_status['volume-max'])
  }
  if ("loop-file" in status_updates
    ||"loop-playlist" in status_updates){
    setLoop(new_status["loop-file"], new_status["loop-playlist"])
  }
  if ("fullscreen" in status_updates){
    setFullscreenButton(new_status['fullscreen'])
  }
  if ("chapters" in status_updates
    ||"chapter" in status_updates
    ||"chapter-metadata" in status_updates){
    setChapter(new_status['chapters'], new_status['chapter'], new_status['chapter-metadata'])
  }

  if ("chapter" in status_updates && mpv_status['chapter'] == -1){ /* See (4*) */
    updateCapterMarks(new_status['chapters'], new_status['chapter'], new_status['chapter-metadata'],
      new_status['chapter-list'], new_status['duration'])
  } else if ("chapters" in status_updates){
    updateCapterMarks(new_status['chapters'], new_status['chapter'], new_status['chapter-metadata'],
      new_status['chapter-list'], new_status['duration'])
  }
  if ("playlist" in status_updates){
    updateNotification(new_status)
    updatePlaylist(new_status['playlist'],
      mpv_status['playlist'], new_status['pause'])
  }
  if ("pause" in status_updates){
    setPlayPause(new_status['pause'])
    updateNotification(new_status)
    updatePlaylistOnPause(new_status['playlist'],
      new_status['pause'], mpv_status['pause'])
  }
  if ("metadata" in status_updates
    ||"filename" in status_updates){
    updateNotification(new_status)
  }

  mpv_status = new_status
}

function status_init_ws(){
  ws = new WebSocket('ws://'+window.location.host+'/ws');

  ws.__unhandled_data = []
  ws.onmessage=function(ev){
    try {
      // Strip websocket metadata
      //var metadata = Uint16Array.from(ev.data.slice(0,8)) // Wrong?!
      metadata = [
        (ev.data.charCodeAt(1) << 8) + ev.data.charCodeAt(0),
        (ev.data.charCodeAt(3) << 8) + ev.data.charCodeAt(2),
      ]
      //console.log("Num chunks: " + metadata[0])
      //console.log("Chunk   ID: " + metadata[1])
    } catch (e) {
      //e instanceof SyntaxError
      console.log(e.name + ": " + e.message)
      console.log("Input: " + ev.data)
      return
    }

    // Remove pending data.
    if ( metadata[1] == 0 && ws.__unhandled_data.length > 0 ){
      console.log("Websocket communication error. New set of chunks begins,"
        + "but previous one was not completed?!")
      console.log("Old data:\n" + ws.__unhandled_data)
      console.log("New data:\n" + ev.data.slice(4))
      console.log("New Metadata: chunks/id= " + metadata[0] + "/" + metadata[1])

      ws.__unhandled_data = []
    }

    if ( 1 == metadata[0] ){
      // Just one chunk, skip call of join()...
      ws.oncompletejson(ev.data.slice(4))
    }else{
      // Cache data
      ws.__unhandled_data[metadata[1]] = ev.data.slice(4)

      // Handle data if complete
      if ( ws.__unhandled_data.length == metadata[0] ){
        var data = ws.__unhandled_data.join('')
        ws.__unhandled_data = []
        ws.oncompletejson(data)
      }
    }
  }

  // Triggered after complete json-struct was recived
  ws.oncompletejson=function(data){
    try {
      var json = JSON.parse(data)
    } catch (e) {
      //e instanceof SyntaxError
      console.log(e.name + ": " + e.message)
      console.log("Input: " + data)
      return
    }

    if ("status_info" in json){
      var  info = json["status_info"]
      if (!info.connection){
        console.log("Websocket error on connection:" + info)
      }
      if ("page_title" in info) {
        document.title = info["page_title"]
      }
    }

    if ("status" in json){
      mpv_status = json["status"]
      handleStatusResponse(mpv_status)
    }
    if ("status_diff" in json){

      // Collect updates in global var
      Object.assign(window.mpv_outstanding_status.updates, json["status_diff"])
      window.mpv_outstanding_status.pending = true
      checkForStatusUpdate(false)
    }
    if ("result" in json){
      DEBUG && console.log(JSON.stringify(json))
    }
  }

  ws.onopen = function(ev){
    connected = true;
    DEBUG && console.log("Websocket connected")
  }
  ws.onclose = function(ev){ // CloseEvent
    DEBUG && console.log("Websocket closed with code " + ev.code)
    if (connected) {
      connected = false;
      if (ev.code != 1001 /* GOING_AWAY reply. No need for further rendering */){
        print_disconnected()
      }
    }

    if (!window.tab_in_background) {
      window.mpv_outstanding_status.restart = setTimeout(function() {
        console.log("Try reconnecting websocket")
        status_init_ws()
      }, 5000)
    }
  }
  //ws.onerror = ws.onclose; // not required for reconnections

}

function print_disconnected(){
  document.getElementById("title").innerHTML = "<span class='info'>Not connected to MPV!</span>"
  document.getElementById("artist").innerText = ""
  document.getElementById("album").innerText = ""
  setPlayPause("yes")
}

/*
function status(){
  var request = new XMLHttpRequest()
  request.open("get", "/api/status")

  request.onreadystatechange = function() {
    if (request.readyState === 4 && request.status === 200) {
      var json = JSON.parse(request.responseText)
      handleStatusResponse(json)
    } else if (request.status === 0) {
      document.getElementById("title").innerHTML = "<h1><span class='error'>Couldn't connect to MPV!</span></h1>"
      document.getElementById("artist").innerText = ""
      document.getElementById("album").innerText = ""
      setPlayPause("yes")
    }
  }
  request.send(null)
}
*/

function audioLoad() {
  if (!use_dummy_audio_element) return;

  DEBUG && console.log('Loading dummy audio')
  var audio = document.getElementById("audio")
  audio.load()

  /* Handle Events triggerd on Lock-Screen of smartphone.
   *
   * Three cases of invoking:
   * 1. Event is triggered by
   * a change on the server side, we does not react
   * (mpv_status["paused"] is already as expected).
   *
   * 2. Event is triggerd by hitting the play/pause GUI button,
   * => Will handled like case 3.
   *
   * 3. Event is triggerd on Lock-Screen or <audio>-Controls
   * => The status on the server side may differ and we had
   * to send a api call.
   */
  audio.addEventListener('play', ()=> {
    if (!block.doublePause){
      updatePlayPauseButton("no")
      send("play")
    }
    block.doublePause = false
  }, false)
  audio.addEventListener('pause', ()=> {
    if (!block.doublePause){
      updatePlayPauseButton("yes")
      send("pause")
    }
    block.doublePause = false
  }, false)
}
window.addEventListener('load', audioLoad, false)

function audioPlay() {
  if (!use_dummy_audio_element) return;

  var audio = document.getElementById("audio")
  if (audio.paused) {
    block.doublePause = true
    audio.play()
      .then(_ => { __mediaSession();
        DEBUG && console.log('Playing dummy audio'); })
      .catch(error => { DEBUG && console.log(error) })
    navigator.mediaSession.playbackState = "playing"
    // (Almost redundant line. Value will be overwritten with feedback of mpv)
  }
}

function audioPause() {
  if (!use_dummy_audio_element) return;

  var audio = document.getElementById("audio")
  if (!audio.paused) {
    block.doublePause = true;
    DEBUG && console.log('Pausing dummy audio')
    audio.pause()
    navigator.mediaSession.playbackState = "paused"
    // (Almost redundant line. Value will be overwritten with feedback of mpv)
  }
}

function setupNotification() {
  __mediaSession()
}

function trim_title_string(s, max_len, sub_char, end_char){
  /* Trim string s by estimation of unrequired parts
   * and finally cut at max_len chars. */

  if (arguments.length < 4) end_char = '…';
  if (arguments.length < 3) sub_char = '…';

  //if (s.length <= max_len ) return s;

  s = s.replace(REGEXS['brackets'], sub_char)
  s = s.replace(REGEXS['checksumBeforeExtension'], RegExp.$1)

  if (s.length > max_len ){
    s = s.replace(REGEXS['extension'], end_char)
  }

  if (s.length > max_len ){
    s = s.substr(0, max_len)
    if (s.charAt(s.length-1) != end_char)
      s = s.concat(end_char)
  }

  // Cleanup
  s = s.replace(REGEXS['cleanup_dots'], sub_char)

  return s;
}

/* To get one-liner... */
function setClass(el, bDisplay, classname){
  if (arguments.length < 3) classname = 'hidden';
  if (bDisplay == true ){
    el.classList.remove(classname)
  }else{
    el.classList.add(classname)
  }
}

function displayElementClass(cls_of_elements, bDisplay, classname){
  // Note: We didn't change the css class property, but add/remove
  // class '.hidden'
  // So be careful if this is called twice for the same element
  // (one adds 'hidden' and one removes…)
  //
  if (arguments.length < 3)
    classname = 'hidden'

  let classElements = document.getElementsByClassName(cls_of_elements);
  [].slice.call(classElements).forEach(function(div) {
    setClass(div, bDisplay, classname)
  })
}

function __changeControls(el, nearby_element_names, bDisplay, classname){
  /* Show/hide elInfo
   * and add/remove 'with_info' class to all elements of nearby_element_names */
  setClass(el, bDisplay, 'hidden')

  nearby_element_names.forEach( nearby_names => {
    document.getElementsByName(nearby_names).forEach( div => {
      setClass(div, !bDisplay, classname)
    })
  })
}
function showMiddleControl(elMiddle, nearby_element_names, bDisplay){
  /* Show/hide middle button
   * and add/remove 'with_mid' class to all elements of nearby_element_names */
  __changeControls(elMidle, nearby_element_names, bDisplay, 'with_mid')
}
function showInfoForControls(elInfo, nearby_element_names, bDisplay){
  /* Show/hide elInfo
   * and add/remove 'with_info' class to all elements of nearby_element_names */
  __changeControls(elInfo, nearby_element_names, bDisplay, 'with_info')
}


function updateNotification(mpv_status) {
  if ('mediaSession' in navigator) {
    navigator.mediaSession.metadata = new MediaMetadata({
      title: window.metadata.title,
      artist: window.metadata.artist,
      album: window.metadata.album,
      artwork: [
        { src: '/favicons/android-chrome-192x192.png', sizes: '192x192', type: 'image/png' },
        { src: '/favicons/android-chrome-512x512.png', sizes: '512x512', type: 'image/png' }
      ]
    })

    navigator.mediaSession.setPositionState({
      duration: mpv_status["duration"],
      position: Math.max(0, Math.min(mpv_status["time-pos"], mpv_status["duration"])),
      playbackRate: mpv_status["speed"] //1.0
    })

    if ("pause" in mpv_status && mpv_status["pause"] == "yes" ){
      navigator.mediaSession.playbackState = "paused"
    }else{
      navigator.mediaSession.playbackState = "playing"
    }
  }
}

// To check log on Mobile-Browser.
// Clearing with double click or double touch.
function logging_in_page(){
  var log = document.createElement('div')
  log.style.cssText = `
    position: absolute;
    top:0em; left:0em;
    width:90%; height: 20%;
    background-color:rgba(0,0,0,0.8);
    color:lightgrey;
    z-index: 1002; overflow:scroll;
  `;
  console.log = function(s) {
    if (typeof(s) == 'object'){
      var t = JSON.stringify(s)
      if (t.length>100) t = t.substr(0,99)+'…'
    }else{
      var t = s.valueOf()
    }
    log.innerHTML += "<br />" + t
    log.scrollTo({'top': 10000})
  }
  log.addEventListener('dblclick',
    function(evt) { this.innerHTML=''})
  log.addEventListener('touchstart',
    function(evt) {
      if (evt.targetTouches.length == 2){this.innerHTML=''}
    })
  document.getElementsByTagName('body')[0].appendChild(log)
}

/* To satisfy Content-Security-Policy define
 * onClick/onTouch/etc events here.
 *
 * Using name instead of id is possible because some Events are
 * triggered by multiple buttons, e.g. 'togglePlayPause'.
 *
 * If longpress handler is used, prefer 'mouseup'/'pointerup' instead of
 * 'mousedown' events. Otherwise the shortclick event will always trigger.
 *
 * If fast multiple clicks needed, use 'pointerup' instead of 'mouseup'.
 * Otherwise, it will just trigger once on touch devices.
 * */
function add_button_listener() {
  const btnEvents = [
    // Element id/name, event name, handler, optional longpress handler
    ['togglePlayPause', 'click', function (evt) {togglePlayPause() }],
    ['playlistLoopCycle', 'click', function (evt) {playlist_loop_cycle() }],
    ['playlistShuffle', 'click', function (evt) {send('playlist_shuffle') }],
    ['togglePlaylist', 'click', function (evt) {togglePlaylist() }],
    ['share_selector', 'change', function (evt) {share_change(evt.target) }],
    ['shareSortingAlpha', 'click', function (evt) {share_change_sorting('alpha'); sortShareList(); }],
    ['shareSortingDate', 'click', function (evt) {share_change_sorting('date'); sortShareList(); }],
    ['toggleShares', 'click', function (evt) {toggleShares() }],
    ['playlistPrev', 'pointerup', function (evt) {send('playlist_prev') },
      function(evt) {touchmenu.prev_files(evt)} ],
    ['playlistNext', 'pointerup', function (evt) {send('playlist_next') },
      function(evt) {touchmenu.next_files(evt)} ],
    ['seekBack1', 'pointerup', function (evt) {send('seek', '-5') },
      function(evt) {touchmenu.seek_menu(evt, [-30, -60, -180, -600, -1800])} ],
    ['seekForward1', 'pointerup', function (evt) {send('seek', '10') },
      function(evt) {touchmenu.seek_menu(evt, [30, 60, 180, 600, 1800])} ],
    ['seekBack2', 'pointerup', function (evt) {send('seek', '-60') }],
    ['seekForward2', 'pointerup', function (evt) {send('seek', '120') }],
    ['chapterBack', 'pointerup', function (evt) {send('add_chapter', '-1') },
      function(evt) {touchmenu.prev_chapters(evt)} ],
    ['chapterForward', 'pointerup', function (evt) {send('add_chapter', '1') },
      function(evt) {touchmenu.next_chapters(evt)} ],
    ['playbackSpeed1', 'pointerup', function (evt) {send('increase_playback_speed', '0.9') }],
    ['playbackSpeed2', 'pointerup', function (evt) {send('increase_playback_speed', '1.1') }],
    ['playbackSpeedReset', 'click', function (evt) {
      setPlaybackSpeedButtons(1.0); // anticipate reply of reset_playback_speed for fast gui reaction.
      send('reset_playback_speed') }],
    ['subDelay1', 'pointerup', function (evt) {send('add_sub_delay', '-0.05') }],
    ['subDelay2', 'pointerup', function (evt) {send('add_sub_delay', '0.05') }],
    ['audioDelay1', 'pointerup', function (evt) {send('add_audio_delay', '-0.05') }],
    ['audioDelay2', 'pointerup', function (evt) {send('add_audio_delay', '0.05') }],
    ['toggleFullscreen', 'click', function (evt) {send('fullscreen') }],
    ['cycleAudioDevice', 'pointerup', function (evt) {send('cycle', 'audio-device') }],
    ['cycleSub', 'pointerup', function (evt) {send('cycle', 'sub') },
      function(evt) {touchmenu.list_subtitle(evt)} ],
    ['cycleAudio', 'pointerup', function (evt) {send('cycle', 'audio') },
      function(evt) {touchmenu.list_audio(evt)} ],
    ['cycleVideo', 'pointerup', function (evt) {send('cycle', 'video') },
      function(evt) {touchmenu.list_video(evt)} ],
    ['mediaPosition', 'change', function (evt) {
      var slider = evt.currentTarget
      send("set_position", slider.value)
      window.block.disable('posSlider')
    }],
    ['mediaPosition', 'input', function (evt) {
      window.block.enable('posSlider')
      var slider = evt.currentTarget
      var pos = document.getElementById("position")
      pos.innerText = format_time(slider.value)
    }],
    ['mediaVolume', 'change', function (evt) {
      var slider = evt.currentTarget
      send("set_volume", slider.value)
      window.block.disable('volSlider')
    }],
    ['mediaVolume', 'input', function (evt) {
      window.block.enable('volSlider')
      var slider = evt.currentTarget
      var vol = document.getElementById("volume")
      vol.innerText = slider.value + "%"
    }],


    ['quitMpv', 'click', function (evt) {send('quit') }],
    ['overlay', 'click', function (evt) {
      if(evt.target == this) togglePlaylist();
      evt.stopPropagation();
    }],
  ]

  btnEvents.forEach( x => {
    let els = document.getElementsByName(x[0])
    let el = document.getElementById(x[0])
    let count = els.length;
    if (el) {
      count++;
    }
    if (count == 0){
      console.log(`No button named '${x[0]}' found to add event!`)
    }
    if (window.longpress) {
      longpress.addEventHandler(els, x[1], x[2], x[3])
      if (el) {
        longpress.addEventHandler([el], x[1], x[2], x[3])
      }
    }else{
      // Fallback on non-touchmenu-case
      console.log("TODO")
    }
  })
}

window.addEventListener('keydown', webui_keydown, true) /* capture to skip scrolling on overlays*/
window.addEventListener('load', status_init_ws, false)
window.addEventListener('load', add_button_listener, false)
//status_ws()
//setInterval(function(){status();}, 1000)


// prevent zoom-in on double-click
// https://stackoverflow.com/questions/37808180/disable-viewport-zooming-ios-10-safari/38573198#38573198
var lastTouchEnd = 0
document.addEventListener('touchend', function (evt) {
  var now = (new Date()).getTime();
  if (now - lastTouchEnd <= 300) {
    evt.preventDefault()
  }
  lastTouchEnd = now
}, false)

if (DEBUG) {
  // For testing long touch events in 'FF + touch simulation':
  // Otherwise the right click simulation aborts my long press detection.
  window.addEventListener("contextmenu", function(e) { e.preventDefault(); })
}
if (DEBUG_MOBILE) {
  logging_in_page()
}
