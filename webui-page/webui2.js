
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
    clearTimeout(mpv_outstanding_status.restart)
    status_init_ws()
    return
  }
  if (!mpv_outstanding_status.pending) return;

  var _handler = function() {
    if (tab_in_background) {
      //console.log('Wait at ' + Date.now())
      mpv_outstanding_status.timer = setTimeout(
        _handler, mpv_outstanding_status.delay)
      return
    }

    mpv_outstanding_status.timer = null

    //TODO Prepare async handling by storing update in local var (!?)
    var updates = mpv_outstanding_status.updates
    mpv_outstanding_status.updates = {}
    //console.log('Handle update at ' + Date.now())
    handleStatusUpdate(updates)
  }

  if (immedialty){
    clearTimeout(mpv_outstanding_status.timer)
    _handler()
  }else if (mpv_outstanding_status.delay == 0 ){
    _handler()
  }else{
    if (mpv_outstanding_status.timer == null ){
      mpv_outstanding_status.timer = setTimeout(
        _handler, mpv_outstanding_status.delay)
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
})
// ===================================

function setTrackList(tracklist) {
  window.audios.selected = 0
  window.audios.count = 0
  window.subs.selected = 0
  window.subs.count = 0
  window.videos.selected = 0
  window.videos.count = 0
  for (let i = 0; i < tracklist.length; i++){
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
  if (window.videos.count > 1 || (window.videos.count > 0 && window.videos.selected == 0)) {
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
  if (window.audios.count > 1 || (window.audios.count > 0 && window.audios.selected == 0)) {
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
  for (let i = 0; i < playlist.length; i++){
    if (playlist[i].hasOwnProperty('current' /* or 'playing' ?!*/)) {
      return i
    }
  }
  // This is ok with idle=yes
  DEBUG && console.log("Can not detect playlist entry!")
  return -1
}


function print_idle(){
  var title = "Idle…"
  window.metadata.title = title
  window.metadata.artist = ''
  window.metadata.album = ''

  document.getElementById("title").innerText = window.metadata.title
  document.getElementById("title").setAttribute('title', title)

  document.getElementById("artist").innerText = window.metadata.artist
  document.getElementById("album").innerText = window.metadata.album
}

function setMetadata(metadata, playlist, filename, new_status) {
  // try to gather the track number
  metadata = metadata?metadata:{}; // null to {}
  var track = ''

  if (check_idle(new_status) ){
    return print_idle()
  }

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
  if (pl_index >= 0 && playlist[pl_index].title) {
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
    })

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


function setChapter(num_chapters, chapter, metadata) {
  var chapterContent = document.getElementById('chapterContent')
  var chapter_title = ''
  if (chapter < /* not <=, -1 is value before first (named) chapter begins. Some podcast did not set first chapter on start of file */ -1 || chapter >= num_chapters) chapter = 0
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

  ;[].slice.call(loopButton).forEach(function (div) {
    div.innerHTML = html
    div.setAttribute("value", value)
    })

}

function handleStatusResponse(json) {
  setMetadata(json['metadata'], json['playlist'], json['filename'], json)
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
    json['chapter-list'], json['duration'], json['time-pos'])
}

function handleStatusUpdate(status_updates) {
  let new_status = Object.assign({}, mpv_status, status_updates)
  mpv_outstanding_status.pending = false
  if ("metadata" in status_updates
    ||"playlist" in status_updates
    ||"filename" in status_updates){
    setMetadata(new_status['metadata'], new_status['playlist'], new_status['filename'], new_status)
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
      new_status['chapter-list'], new_status['duration'], new_status['time-pos'])
  } else if ("chapters" in status_updates){
    updateCapterMarks(new_status['chapters'], new_status['chapter'], new_status['chapter-metadata'],
      new_status['chapter-list'], new_status['duration'], new_status['time-pos'])
  } else if (delayed_chapter_marks && new_status['time-pos'] >= 0.1){
    delayed_chapter_marks = false
    updateCapterMarks(new_status['chapters'], new_status['chapter'], new_status['chapter-metadata'],
      new_status['chapter-list'], new_status['duration'], new_status['time-pos'])
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
  ws = new WebSocket('ws://'+window.location.host+'/ws')

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
      Object.assign(mpv_outstanding_status.updates, json["status_diff"])
      mpv_outstanding_status.pending = true
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
        clearTimeout(mpv_outstanding_status.timer)
        print_disconnected()
        hide_overlays()
      }
    }

    if (!window.tab_in_background) {
      mpv_outstanding_status.restart = setTimeout(function() {
        console.log("Try reconnecting websocket")
        status_init_ws()
      }, 5000)
    }

    ws = null
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

  let classElements = document.getElementsByClassName(cls_of_elements)
  ;[].slice.call(classElements).forEach(function(div) {
    setClass(div, bDisplay, classname)
  })
}

function __changeControls(el, nearby_element_names, bDisplay, classname){
  /* Show/hide elInfo
   * and add/remove 'with_info' class to all elements of nearby_element_names */
  setClass(el, bDisplay, 'hidden')

  nearby_element_names.forEach( nearby_el_name_or_id => {
    setClass(document.getElementById(nearby_el_name_or_id), !bDisplay, classname)
    document.getElementsByName(nearby_el_name_or_id).forEach( div => {
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

function elementsByNameOrId(name_or_id){
  return document.querySelectorAll(`[name=${name_or_id}], #${name_or_id}`)
  // Node merging .getElementsByName() and .getElementById() isn't
  // possible without converting results into real Arrays…
  // using the combined query selector avoids this problem.
}


function init(){
  Object.assign(overlays,
    {
      "overlay1": togglePlaylist,
      "overlay2": toggleShares,
    })

  status_init_ws()
  add_button_listener()

  USE_SWIPES && add_main_swipes()
}

window.addEventListener('keydown', webui_keydown, true) /* capture=true to skip scrolling on overlays*/
window.addEventListener('load', init, false)


// prevent zoom-in on double-click
// https://stackoverflow.com/questions/37808180/disable-viewport-zooming-ios-10-safari/38573198#38573198
var lastTouchEnd = 0
document.addEventListener('touchend', function (evt) {
  var now = (new Date()).getTime()
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
  window.addEventListener('load', logging_in_page, false)
}
