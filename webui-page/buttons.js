
function setFullscreenButton(fullscreen) {
    if (fullscreen === 'yes') {
        var fullscreenText = 'Fullscreen off'
    } else {
        fullscreenText = 'Fullscreen on'
    }
    document.getElementById("fullscreenButton").innerText =
        fullscreenText
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

function update_sort_buttons()
{
  function get_fa_nodes(css_class1, css_class2){
    /* Return children of class2 from elements of class1. */
    var els = document.getElementsByClassName(css_class1)
    var out = [];
    [].slice.call(els).forEach(function (div) {
      var els2 = div.getElementsByClassName(css_class2)
      out = out.concat(Array.from(els2))
    })
    return out
  }

  for (let sname in sortings ){
    var s = sortings[sname]
    var cur = s.active
    DEBUG_SORTINGS && console.log(`Change Icon from ${sname} to ${cur}`)
    var prev = (s.dirs.length + cur - 1) % s.dirs.length
    var x = get_fa_nodes("shareSortButton_" + sname, "fas")
    x.forEach(function (fasEl) {
      fasEl.classList.remove(...s["icons"])
      fasEl.classList.add(s["icons"][cur])
    })
  }
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
    ['browseShares', 'click', function (evt) {browseShares() }],
    ['toggleShares', 'click', function (evt) {toggleShares() }],
    ['playlistPrev', 'pointerup', function (evt) {send('playlist_prev') },
      function(evt) {touchmenu.prev_files(evt)} ],
    ['playlistNext', 'pointerup', function (evt) {send('playlist_next') },
      function(evt) {touchmenu.next_files(evt)} ],
    ['seekBack1', 'pointerup', function (evt) {send('seek', evt.target.getAttribute('seek') || '-5') },
      function(evt) {touchmenu.seek_menu(evt)} ],
    ['seekForward1', 'pointerup', function (evt) {send('seek', evt.target.getAttribute('seek') || '10') },
      function(evt) {touchmenu.seek_menu(evt)} ],
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

    ['playlistAdd', 'click', send_url_input],
    ['playlistAdd_input', 'keyup', function (evt) {
      //console.log(evt)
      if (evt.key === "Enter") {
        send_url_input(evt)
      }
    }],

    ['quitMpv', 'click', function (evt) {send('quit') }],
  ]

  btnEvents.forEach( x => {
    let els = elementsByNameOrId(x[0])
    if (els.length == 0){
      console.log(`No button named '${x[0]}' found to add event!`)
    }
    if (window.longpress) {
      longpress.addEventHandler(els, x[1], x[2], x[3])
    }else{
      // Fallback on non-touchmenu-case: Use only shortpress handle
      els.forEach(el => {
        el.addEventListener(x[1], x[2])
      })
    }
  })
}

