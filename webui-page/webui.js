var DEBUG = true,
    metadata = {},
    subs = {},
    audios = {},
	  blockDoublePause = false,
    blockPosSlider = false,
    blockVolSlider = false

var ws = null
var mpv_status = {}

/* Collect [num] milliseconds status updates before 
 * they will be used to update the page.
 * Use 0 to update immediately. 
 */
var mpv_outstanding_status = {
	delay: 500,
	updates: {},
	timer: null
}


function send_ws(command, ...params){

	api = {
		command: command,
		param1: params[0] || "",
		param2: params[1] || ""
	}

  //DEBUG && console.log('Sending command: ' + api.command + ' - param1: ' + api.param1 + ' - param2: ' + api.param2)

	if (ws){
		// TODO: Implement json-parser on server side?!
		// At the moment we send simply send the url scheme
		uri = String(api.command) + "/" + String(api.param1) + "/" + String(api.param2)
		console.log("Send " + uri)
		ws.send(uri)
		//ws.send(JSON.stringify(api))
	}
	
}

function basename(path){
	var lpath = path.split('/')
	// remove "", e.g for '/' at filename end.
	lpath = lpath.filter(item => item)
	return lpath[lpath.length - 1]
}

function send(command, param){
	return send_ws(command, param)

  DEBUG && console.log('Sending command: ' + command + ' - param: ' + param)
  var path = 'api/' + command
  if (param !== undefined)
    path += "/" + param

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
      td_left.classList.add('active')
      td_2.classList.add('active')
    } else {
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
    title = basename(entry.filename)
  }

  var table = document.createElement('table')
  var tr = document.createElement('tr')
  var td_left = document.createElement('td')
  var td_2 = document.createElement('td')
  var td_right = document.createElement('td')
  table.className = 'playlist'
  tr.className = 'playlist'
  td_2.className = 'playlist'
  td_left.className = 'playlist'
  td_right.className = 'playlist'
  td_2.innerText = title
  if (first === false) {
    var td_3 = document.createElement('td')
    td_3.innerHTML = '<i class="fas fa-arrow-up"></i>'
    td_3.className = 'playlist'
  }
  td_right.innerHTML = '<i class="fas fa-trash"></i>'

  if (entry.hasOwnProperty('playing')) {
    if (pause) {
      td_left.innerHTML = '<i class="fas fa-pause"></i>'
    } else {
      td_left.innerHTML = '<i class="fas fa-play"></i>'
    }

    td_left.classList.add('playing')
    td_left.classList.add('violet')
    td_2.classList.add('playing')
    td_2.classList.add('violet')
      first || td_3.classList.add('violet')
    td_right.classList.add('violet')

  } else {
    td_left.classList.add('gray')
    td_2.classList.add('gray')
    first || td_3.classList.add('gray')
    td_right.classList.add('gray')

    td_left.onclick = td_2.onclick = function(arg) {
        return function() {
            send("playlist_jump", arg)
            send("play");  // remove pause
            return false
        }
    }(position)

    td_left.addEventListener("mouseover", function() {setActive(true)})
    td_left.addEventListener("mouseout", function() {setActive(false)})
    td_2.addEventListener("mouseover", function() {setActive(true)})
    td_2.addEventListener("mouseout", function() {setActive(false)})

    td_left.addEventListener("click", blink)
    td_2.addEventListener("click", blink)
  }

  if (first === false) {
    td_3.onclick = function (arg) {
      return function () {
        send("playlist_move_up", arg)
        return false
      }
    }(position)
  }

  td_right.onclick = function(arg) {
      return function() {
          send("playlist_remove", arg)
          return false
      }
  }(position)

  tr.appendChild(td_left)
  tr.appendChild(td_2)
  first || tr.appendChild(td_3)
  tr.appendChild(td_right)
  table.appendChild(tr)
  return table
}

function populatePlaylist(json, pause) {
  var playlist = document.getElementById('playlist')
  playlist.innerHTML = ""

  var first = true
  for(var i = 0; i < json.length; ++i) {
    playlist.appendChild(createPlaylistTable(json[i], i, pause, first))
    if (first === true) {
      first = false
    }
  }

	playlist.addEventListener('click', function _listener(evt) {
		if( evt.target == playlist ){
			playlist.removeEventListener('click', _listener)
			togglePlaylist()

		}
	}, false)
}



function webui_keydown(e) {
  var bindings = [
    {
      "key": " ",
      "code": 32,
      "command": "toggle_pause"
    },
    {
      "key": "ArrowRight",
      "code": 39,
      "command": "seek",
      "param1": "10"
    },
    {
      "key": "ArrowLeft",
      "code": 37,
      "command": "seek",
      "param1": "-10"
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
  ]
  for (var i = 0; i < bindings.length; i++) {
    if (e.keyCode === bindings[i].code || e.key === bindings[i].key) {
      send(bindings[i].command, bindings[i].param1, bindings[i].param2)
      return false
    }
  }
}

function format_time(seconds){
  var date = new Date(null);
  date.setSeconds(seconds)
  return date.toISOString().substr(11, 8)
}

function setFullscreenButton(fullscreen) {
    if (fullscreen) {
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
    }
  }
  document.getElementById("nextSub").innerText = 'Next sub ' + window.subs.selected + '/' + window.subs.count
  document.getElementById("nextAudio").innerText = 'Next audio ' + window.audios.selected + '/' + window.audios.count
}

function setMetadata(metadata, playlist, filename) {
  // try to gather the track number
	metadata = metadata?metadata:{}; // null to {}
	var track = ''
  if (metadata['track']) {
    track = metadata['track'] + ' - '
  }

  // try to gather the playing playlist element
	var pl_title = null
  for (var i = 0; i < playlist.length; i++){
    if (playlist[i].hasOwnProperty('playing')) {
       pl_title = playlist[i].title
    }
  }
  // set the title. Try values in this order:
  // 1. title set in playlist
  // 2. metadata['title']
  // 3. metadata['TITLE']
  // 4. filename
  if (pl_title) {
    window.metadata.title = track + pl_title
  } else if (metadata['title']) {
    window.metadata.title = track + metadata['title']
  } else if (metadata['TITLE']) {
    window.metadata.title = track + metadata['TITLE']
  } else {
    window.metadata.title = track + filename
  }

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

  document.getElementById("title").innerHTML = window.metadata.title
  document.getElementById("artist").innerHTML = window.metadata.artist
  document.getElementById("album").innerHTML = window.metadata.album
}

function setPosSlider(position, duration) {
  var slider = document.getElementById("mediaPosition")
  var pos = document.getElementById("position")
  slider.max = duration
  if (!window.blockPosSlider) {
    slider.value = position
  }
  pos.innerHTML = format_time(slider.value)
}

document.getElementById("mediaPosition").onchange = function() {
  var slider = document.getElementById("mediaPosition")
  send("set_position", slider.value)
  window.blockPosSlider = false
}

document.getElementById("mediaPosition").oninput = function() {
  window.blockPosSlider = true
  var slider = document.getElementById("mediaPosition")
  var pos = document.getElementById("position")
  pos.innerHTML = format_time(slider.value)
}

function setVolumeSlider(volume, volumeMax) {
  var slider = document.getElementById("mediaVolume")
  var vol = document.getElementById("volume")
  if (!window.blockVolSlider) {
    slider.value = volume
    slider.max = volumeMax
  }
  vol.innerHTML = slider.value + "%"
}

document.getElementById("mediaVolume").onchange = function() {
  var slider = document.getElementById("mediaVolume")
  send("set_volume", slider.value)
  window.blockVolSlider = false
}

document.getElementById("mediaVolume").oninput = function() {
  window.blockVolSlider = true
  var slider = document.getElementById("mediaVolume")
  var vol = document.getElementById("volume")
  vol.innerHTML = slider.value + "%"
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
		navigator.mediaSession.setActionHandler('seekbackward', function() {send('seek', '-10');})
		navigator.mediaSession.setActionHandler('seekforward', function() {send('seek', '10');})
		navigator.mediaSession.setActionHandler('previoustrack', function() {send('playlist_prev');})
		navigator.mediaSession.setActionHandler('nexttrack', function() {send('playlist_next');})

		navigator.mediaSession.setPositionState({
			duration: mpv_status["duration"],
			position: mpv_status["time-pos"],
			playbackRate: 1.0
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

function setChapter(chapters, chapter) {
  var chapterElements = document.getElementsByClassName('chapter')
  var chapterContent = document.getElementById('chapterContent')
  if (chapters === 0) {
    [].slice.call(chapterElements).forEach(function (div) {
      div.classList.add('hidden')
    })
    chapterContent.innerText = "0/0"
  } else {
    [].slice.call(chapterElements).forEach(function (div) {
      div.classList.remove('hidden')
    })
    chapterContent.innerText = chapter + 1 + "/" + chapters
  }
}

function setSubDelay(fDelay) {
  var subElements = document.getElementsByClassName('sub-delay')
  var subContent = document.getElementById('sub-delay')
	if ( fDelay == 0 ){
    [].slice.call(subElements).forEach(function (div) {
      div.classList.add('hidden')
		})
	}else{
		subContent.innerHTML = parseInt(1000*fDelay) + ' ms';
    [].slice.call(subElements).forEach(function (div) {
      div.classList.remove('hidden')
		})
	}
}

function setAudioDelay(fDelay) {
  var audioElements = document.getElementsByClassName('audio-delay')
  var audioContent = document.getElementById('audio-delay')
	if ( fDelay == 0 ){
    [].slice.call(audioElements).forEach(function (div) {
      div.classList.add('hidden')
		})
	}else{
		audioContent.innerHTML = parseInt(1000*fDelay) + ' ms';
    [].slice.call(audioElements).forEach(function (div) {
      div.classList.remove('hidden')
		})
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
  document.getElementById("duration").innerHTML =
    '&nbsp;'+ format_time(json['duration'])
  document.getElementById("playtime-remaining").innerHTML =
    "-" + format_time(json['playtime-remaining'])


	// derive position value if not given
	/* // bad idea because playback-time is scaled by speed, but duration not
	if (!("playback-time" in json ) && "duration" in json){
		d = json["duration"] - json["playtime-remaining"]
		json["playback-time"] = (d>=0.0?d:0.0)
	}*/

	setSubDelay(json['sub-delay'])
	setAudioDelay(json['audio-delay'])
  setPlayPause(json['pause'])
  //setPosSlider(json['playback-time'], json['duration'])
  setPosSlider(json['time-pos'], json['duration'])
  setVolumeSlider(json['volume'], json['volume-max'])
  setLoop(json["loop-file"], json["loop-playlist"])
  setFullscreenButton(json['fullscreen'])
  setChapter(json['chapters'], json['chapter'])
  populatePlaylist(json['playlist'], json['pause'])
  if ('mediaSession' in navigator) {
    setupNotification()
  }
}

function handleStatusUpdate(status_updates) {

	new_status = Object.assign({}, mpv_status, status_updates)
	if ( "metadata" in status_updates 
		||"playlist" in status_updates 
		||"filename" in status_updates ){
		setMetadata(new_status['metadata'], new_status['playlist'], new_status['filename'])
	}
	if ("track-list" in status_updates){
		setTrackList(new_status['track-list'])
	}
	if ("duration" in status_updates){
		document.getElementById("duration").innerHTML =
			'&nbsp;'+ format_time(new_status['duration'])
	}
	if ("playtime-remaining" in status_updates){
		document.getElementById("playtime-remaining").innerHTML =
			"-" + format_time(new_status['playtime-remaining'])

		// derive position value if not given
		/* // bad idea because playback-time is scaled by speed, but duration not
		if (!("playback-time" in status_updates )
			&& "duration" in new_status){
			d = new_status["duration"] - new_status["playtime-remaining"]
			status_updates["playback-time"] = (d>=0.0?d:0.0)
			new_status["playback-time"] = (d>=0.0?d:0.0)
		}*/
	}
	if ("sub-delay" in status_updates){
		setSubDelay(new_status['sub-delay'])
	}
	if ("audio-delay" in status_updates){
		setAudioDelay(new_status['audio-delay'])
	}

	if ("pause" in status_updates){
		setPlayPause(new_status['pause'])
	}
	if ("time-pos" in status_updates
		||"duration" in status_updates ){
		setPosSlider(new_status['time-pos'], new_status['duration'])
	}
	if ("volume" in status_updates
		||"volume-max" in status_updates ){
		setVolumeSlider(new_status['volume'], new_status['volume-max'])
	}
	if ("loop-file" in status_updates
		||"loop-playlist" in status_updates ){
		setLoop(new_status["loop-file"], new_status["loop-playlist"])
	}
	if ("fullscreen" in status_updates){
		setFullscreenButton(new_status['fullscreen'])
	}
	if ("chapters" in status_updates
		||"chapter" in status_updates ){
		setChapter(new_status['chapters'], new_status['chapter'])
	}
	if ("playlist" in status_updates
		||"pause" in status_updates ){
		populatePlaylist(new_status['playlist'], new_status['pause'])
	}
	if ("metadata" in status_updates
		||"playlist" in status_updates 
		||"filename" in status_updates 
		||"pause" in status_updates ){
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
			console.log("Num chunks: " + metadata[0])
			console.log("Chunk   ID: " + metadata[1])
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

		if ("status" in json){
			mpv_status = json["status"]
			handleStatusResponse(mpv_status)
		}
		if ("status_diff" in json){
			var status_updates = json["status_diff"]
			if( mpv_outstanding_status.delay > 0 ){
				// Collect updates
				Object.assign(mpv_outstanding_status.updates, status_updates)

				// Set timer for propagation
				if ( mpv_outstanding_status.timer == null ){
					mpv_outstanding_status.timer = setTimeout(function() {
						mpv_outstanding_status.timer = null
						var updates = mpv_outstanding_status.updates // store in local var
						mpv_outstanding_status.updates = {} // Good idea for async websocket handling?!
						//console.log("Update status! " + Object.keys(updates).length)
						handleStatusUpdate(updates)
					}, mpv_outstanding_status.delay)
				}

			}else{
				handleStatusUpdate(status_updates)
			}
		}
		if ("result" in json){
			DEBUG && console.log(json)
		}
	}

	ws.onclose = function(ev){ // CloseEvent
		DEBUG && console.log("Websocket closed with code " + ev.code)
		print_disconnected()

		mpv_outstanding_status.restart = setTimeout(function() {
			console.log("Try reconnecting websocket")
			status_init_ws()
		}, 5000)
	}
	//ws.onerror = ws.onclose; // not required for reconnections 

}

function print_disconnected(){
	document.getElementById("title").innerHTML = "<h1><span class='error'>Not connected to MPV!</span></h1>"
	document.getElementById("artist").innerHTML = "|"
	document.getElementById("album").innerHTML = "|"
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
      document.getElementById("artist").innerHTML = ""
      document.getElementById("album").innerHTML = ""
      setPlayPause("yes")
    }
  }
  request.send(null)
}
*/

function audioLoad() {
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
		if (!blockDoublePause){
			updatePlayPauseButton("no")
			send("play")
		}
		blockDoublePause = false
	}, false)
	audio.addEventListener('pause', ()=> {
		if (!blockDoublePause){
			updatePlayPauseButton("yes")
			send("pause")
		}
		blockDoublePause = false
	}, false)
}
window.addEventListener('load', audioLoad, false)

function audioPlay() {
  var audio = document.getElementById("audio")
  if (audio.paused) {
		blockDoublePause = true
		audio.play()
			.then(_ => { __mediaSession();
				DEBUG && console.log('Playing dummy audio'); })
			.catch(error => { DEBUG && console.log(error) })
		navigator.mediaSession.playbackState = "playing"
		// (Almost redundant line. Value will be overwritten with feedback of mpv)
  }
}

function audioPause() {
  var audio = document.getElementById("audio")
  if (!audio.paused) {
		blockDoublePause = true;
    DEBUG && console.log('Pausing dummy audio')
    audio.pause()
		navigator.mediaSession.playbackState = "paused"
		// (Almost redundant line. Value will be overwritten with feedback of mpv)
  }
}

function setupNotification() {
	__mediaSession()
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
			position: mpv_status["time-pos"],
			playbackRate: 1.0
		})

		if ("pause" in mpv_status && mpv_status["pause"] == "yes" ){
			navigator.mediaSession.playbackState = "paused"
		}else{
			navigator.mediaSession.playbackState = "playing"
		}
  }
}

window.addEventListener('keydown', webui_keydown, false)
window.addEventListener('load', status_init_ws, false)
//status_ws()
//setInterval(function(){status();}, 1000)


// prevent zoom-in on double-click
// https://stackoverflow.com/questions/37808180/disable-viewport-zooming-ios-10-safari/38573198#38573198
var lastTouchEnd = 0
document.addEventListener('touchend', function (event) {
  var now = (new Date()).getTime();
  if (now - lastTouchEnd <= 300) {
    event.preventDefault()
  }
  lastTouchEnd = now
}, false)
