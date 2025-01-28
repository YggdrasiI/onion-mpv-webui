const DEBUG_TOUCH = true

const options = {
  open_with_long_click: false,     /* If false, menu will open with sort press/click.
                                  * If true, button will triggered by short click
                                  * and menu will be open by longpress.
                                  */
  update_default_entry: false,   /* Selected menu entry will replace default 
                                  * entry for short press.
                                  */
  default_entry_in_menu: false,  /* If false, the shortpress entry is not included 
                                  * in touchmenu list.
                                  */
  show_silbing_menu: true,       /* If true the menu for the 'opposite operation' will also be shown. */
}

const sibling_ids = {
  playlistPrev: "playlistNext",
  playlistNext: "playlistPrev",
  seekBack1: "seekForward1",
  seekForward1: "seekBack1",
  chapterBack: "chapterForward",
  chapterForward: "chapterBack",
}


/* Helper object to create elements with short and long press events */
var longpress = {
  options : {
    ms: 400,
  },
  done: null, /* Tri state flag to control hiding of menu.
                  • true: Last click/touch was long.
                          => Do not close menu on next mouseup/touchend event.
                  • false: short press triggered or already an other
                          mouseup/touchend event occoured.
                          => Hide menu in this state.
                  • null: Menu was already hidden. Nothing to do.
                  */
  timer: null,

  addEventHandler: function(els, evt_type, shortpress_h, longpress_h){
    if (els.length == 0) return;

    // Threat undefined handlers like null
    if (shortpress_h === undefined) shortpress_h = null
    if (longpress_h === undefined) longpress_h = null

    if (!options.open_with_long_click && longpress_h !== null){
      [shortpress_h, longpress_h] = [longpress_h, shortpress_h]
    }

    if (longpress_h != null && !evt_type in ['mouseup', 'pointerup']){
      console.log("Warning, touchmenu defined for event != mouseup/pointerup."
      + "Element: " + (els[0].getAttribute('id')||els[0].getAttribute('name')) )
    }

    if (longpress_h !== null) {
      var fshort = function (evt) {
        if (!longpress.done){ /* null or false */
          shortpress_h(evt)
        }
      }
    }else{
      var fshort = shortpress_h
    }

    els.forEach(el => {el.addEventListener(evt_type, fshort)})

    // This is problematic because 'mouseup' will be fire on touch event, too.
    // Thus the event is fired twice.
    /*if (evt_type == "mouseup") {
      els.forEach(el => {el.addEventListener("touchend", fshort)})
      if (el) {
        el.addEventListener("touchend", fshort)
      }
    }*/


    /* Add handlers for longpresses */
    if (longpress_h !== null) {
      var flong = function (evt) {
        if (!touchmenu._hidden) return;

        longpress.done = false
        longpress.timer = setTimeout(
          function(target) {
            console.log("XXXX " + target.constructor.name)
            return function() {
              if (options.open_with_long_click){
                 longpress.done = true
                longpress_h(target)
              }else{
                if (longpress.done === null){
                  longpress.done = true
                  longpress_h(target)
                }
              }
            }
          }(evt.currentTarget), longpress.options.ms); // evt.currentTarget -> evt. to normalize args between short and long press.
          //}(evt), longpress.options.ms); // evt.currentTarget -> evt. to normalize args between short and long press.
      }

      var flongAbort = function (evt) {
        if (longpress.timer){
          clearTimeout(longpress.timer);
          longpress.timer = null
          //longpress.done = null  // wrong, we need 'false' in 'hideTouchMenu' handler
        }
      }

      els.forEach(el => {
        el.addEventListener('touchstart', flong)
        el.addEventListener('mousedown', flong)
        el.addEventListener('touchend', flongAbort)
        el.addEventListener('mouseup', flongAbort)
      })
    }
  },

}

var touchmenu = {
  options: {
    catch_events_outside: true,
    id: 'touchmenu',
    id2: 'touchmenu2',
  },

  /* Flag set on pointerdown/mousedown/touchstart.
   * Used for 
   *    • Avoid captureEvents if event occoured inside of menu.
   *    • Abort menu hide after touchend.
   */
  touchstart_in_menu: false, 
  /* To abort touchend events of menu after scrolling its elements.
   */
  touchmove_in_menu: false,

  /* Workaround: Some click events still bubbles towards captureEvents() for window
   * This flag marks click event as already handled.
   * (for options.open_with_long_click == false)*/
  click_evt_processed: false,

  _gen_menu_elements: function(){
    var menu_ids = options.show_silbing_menu?[this.options.id2, this.options.id]:[this.options.id];
    [].slice.call(menu_ids).forEach(function (menu_id) {
      menu = document.createElement('div')
      menu.id = menu_id
      menu.classList.add("touchmenu");
      menu.style.cssText = `
        position: absolute;
        display:none;
        `;

      document.getElementsByTagName('body')[0].appendChild(menu)
    })
  },

  _getSiblingTarget: function(primary_id){
    var alt_id = sibling_ids[primary_id]
    if (alt_id) return document.getElementById(alt_id)
  },

  get_element: function(sibling){
    var menu = document.getElementById(sibling?this.options.id2:this.options.id)
    if (menu === null) {
      this._gen_menu_elements()
      menu = document.getElementById(sibling?this.options.id2:this.options.id)
    }
    return menu
  },

  /*get_sibling: function(){
    if (menu === null) {
      this._gen_menu_elements()
    }
    var menu = document.getElementById(this.options.id2)
    return menu
  },*/

  show: function (menu) {
    touchmenu._hidden = false
    lock_slider_state(true)
    if (!menu) {
      menu = this.get_element()
    }
    menu.style.setProperty('display', 'block')
  },

  hide: function(menu){
    console.log("Ho")
    if (touchmenu._hidden) return;

    touchmenu._hidden = true
    longpress.done = null
    lock_slider_state(false)
    if (!menu) {
      menu = this.get_element()
    }
    menu.style.setProperty('display', 'none')

    if (options.show_silbing_menu){
      altMenu = this.get_element(true)
      if (altMenu) altMenu.style.setProperty('display', 'none')
    }
  },

	/* Used keywords: sibling, preferBottomOffset, expand */
  _prepare: function (menu, currentTarget, kwargs){
    /* Shifts 'menu' below or top of 'currentTarget'.
     *
     * • If viewports vertical space above 'currentTarget' is bigger
     *   it selects the top position. This switch can be shifted by
     *   'preferBottomOffset'
     * • The 'menu' got the same width as 'currentTarget'
     * • The maximal 'menu' height is set by CSS.
     *
     * Returns: True if top position was selected.
     */

    kwargs.sibling = kwargs.sibling || false
    kwargs.expand = kwargs.expand || {}
    kwargs.preferBottomOffset = kwargs.preferBottomOffset || 0

		// For _position
		//kwargs.above = kwargs.above !== undefined || undefined
		kwargs.during_resize = kwargs.during_resize || false

    if (this._menu_handler_initialized.hasOwnProperty(menu) === false){
      DEBUG_TOUCH && console.log("Add menu listener")
      //menu.addEventListener('mousedown', this._avoid_menu_hide)
      //menu.addEventListener('touchstart', this._avoid_menu_hide)
      menu.addEventListener('pointerdown', this._avoid_menu_hide)
      menu.addEventListener('touchmove', this._skip_touchend_handler)
      this._menu_handler_initialized[menu] = true
    }
    this._menu_innerHeights[menu] = window.innerHeight

    // Reset flags
    this.touchstart_in_menu = false
    this.touchmove_in_menu = false

    // Clear old content
    menu.style.setProperty('display', 'none')
    menu.replaceChildren()

    // Eval new position. Here the above flag is undefined, but defined in all later calls. We fixing this value because otherwise the order of elements needs to be reversed.
    var above = this._position(menu, currentTarget, kwargs)

    // Define new resize handler (respecting arguments of this function)
		var kwargs_resize = {
			preferBottomOffset: kwargs.preferBottomOffset,
			expand: kwargs.expand,
			above: above,
			during_resize: true}
    var new_handler = function(evt){
      //console.log(`Resize! IH: ${window.innerHeight} OH: ${window.outerHeight} BAR: ${window.locationbar.visible} `)

      if (touchmenu._update_innerHeight) // flag set by orientation event
      {
        touchmenu._update_innerHeight = false
        Object.keys(touchmenu._menu_innerHeights).forEach( (m) => {
            touchmenu._menu_innerHeights[m] = window.innerHeight
        })
      }

      // Update position 
      touchmenu._position(menu, currentTarget, kwargs_resize)
    }
    if (kwargs.sibling){
      // Remove previous resize handler
      window.removeEventListener('resize', this._resize_handler_sibling[menu])

      // Save handler for next remove
      this._resize_handler_sibling[menu] = new_handler

      // Add resize handler
      window.addEventListener('resize', this._resize_handler_sibling[menu])
    }else{
      // Remove previous resize handler
      window.removeEventListener('resize', this._resize_handler[menu])

      // Save handler for next remove
      this._resize_handler[menu] = new_handler

      // Add resize handler
      window.addEventListener('resize', this._resize_handler[menu])
    }

    return above;
  },

  _position: function (menu, currentTarget, kwargs) { //preferBottomOffset, expand, above, during_resize){
		kwargs.expand = kwargs.expand || {}
		//kwargs.during_resize = kwargs.during_resize || false
		kwargs.preferBottomOffset = kwargs.preferBottomOffset || 0
    //console.log("_position called")
    const rect = currentTarget.getBoundingClientRect();

    // FF-Mobile: Respect hiding of address bar
    const addressbar_offset = kwargs.during_resize?0:
			(window.innerHeight - touchmenu._menu_innerHeights[menu])

    if (kwargs.expand['left']){
      const rectL = kwargs.expand['left'].getBoundingClientRect();
      menu.style.setProperty('left', Math.round(
        rectL.left + window.scrollX)+'px')
    }else{
      menu.style.setProperty('left', Math.round(
        rect.left + window.scrollX)+'px')
    }
    if (kwargs.expand['right']){
      const rectR = kwargs.expand['right'].getBoundingClientRect();
      menu.style.setProperty('right', Math.round(
        window.innerWidth - rectR.right - window.scrollX )+'px')
    }else{
      menu.style.setProperty('right', Math.round(
        window.innerWidth - rect.right - window.scrollX)+'px')
    }

    if (kwargs.above || (kwargs.above === undefined && rect.top > window.innerHeight - rect.bottom  + kwargs.preferBottomOffset)) {
      var above = true
      menu.style.removeProperty('top')
      menu.style.setProperty('bottom', Math.round(
        window.innerHeight - rect.top - window.scrollY - addressbar_offset
      )+'px')
      //menu.style.removeProperty('border-top-width')
      //menu.style.setProperty('border-bottom-width', '0.5em')
    }else{
      var above = false
      menu.style.setProperty('top', Math.round(
        rect.bottom + window.scrollY + addressbar_offset
      )+'px')
      menu.style.removeProperty('bottom')
      //menu.style.setProperty('border-top-width', '0.5em')
      //menu.style.removeProperty('border-bottom-width')
    }
    return above
  },

  _add_entry: function (ul, title, handler, idx, playlist_range) {
    var el = document.createElement('li')
    el.classname = "button content playlist-controls touch-entry"
    el.innerText = idx<0?`${title}`:`#${idx} ${title}`
    el.addEventListener("click", handler)
    //el.addEventListener("mouseup", handler)
    /*el.addEventListener("touchend", function (evt){
      if (!touchmenu.touchmove_in_menu){
        DEBUG_TOUCH && console.log("Fire on touchend")
        handler(evt);
      }
      //touchmenu.touchstart_in_menu = false // wrong to delete flag here. We need it in hide menu event
      touchmenu.touchmove_in_menu = false

    })*/
    ul.appendChild(el)
  },

  _add_info: function (menu, text) {
    var el = document.createElement('li')
    el.classname = "touch-info"
    el.innerText = text
    menu.appendChild(el)
  },

  _fill_ul: function(menu, reverse, add_entry_args){
    var ul = document.createElement('ul')
    if (reverse) {
      ul.style.setProperty('flex-direction', 'column-reverse')
    }else{
      ul.style.setProperty('flex-direction', 'column')
    }

    add_entry_args.forEach(a => {
      this._add_entry(ul, a[0], a[1], a[2], a[3])
    })

    menu.appendChild(ul)
    if (ul.children.length > 0 ){
      if (reverse) {
        this.show(menu) // bring showing forward. Otherwise scrolling had no effect.
        ul.children[0].scrollIntoView({alignToTop: false});
      }else{
        ul.children[0].scrollIntoView({alignToTop: true});
      }
    }
  },

  next_files: function show_next_files_menu(currentTarget, kwargs){
		kwargs = kwargs || {}

    if (!options.open_with_long_click && !kwargs.sibling){
      //currentTarget.stopImmediatePropagation() // no effect on captureEvents-function calls?!
      this.click_evt_processed = true;
      currentTarget = currentTarget.currentTarget; // Input is Event
    }
    var menu = this.get_element(kwargs.sibling)
		kwargs.preferBottomOffset = 100
		kwargs.expand = {'left': document.getElementById("playlistPrev")}
    const reverse = this._prepare(menu, currentTarget, kwargs)

    // Search next M files
    const M = 15
    const playlist = mpv_status['playlist']
    if (!playlist) return;

    // Range [A,B)
    const A = current_playlist_index(playlist)+1
    const B = Math.min(A+M, playlist.length)
    if (A == playlist.length){
      this._add_info(menu, "No further entries") // TODO: Did not respect looping
    }else{

      add_entry_args = []
      for(var n=A; n<B; ++n){
        add_entry_args.push([
          playlist_get_title(playlist[n]),
          function (arg) {
            return function (evt) {
              send("playlist_jump", arg)
              send("play")
            }
          }(n), n+1, [A, B]])
      }

      this._fill_ul(menu, reverse, add_entry_args)
    }
    this.show(menu)

    if (options.show_silbing_menu && !kwargs.sibling){
			kwargs.sibling = true
			kwargs.above = !reverse // Forces position flip for sibling menu because they are too wide for the same vertical position.
      var altTarget = this._getSiblingTarget(currentTarget.id)
      if (altTarget) this.prev_files(altTarget, kwargs)
    }
  },

  prev_files: function (currentTarget, kwargs){
		kwargs = kwargs || {}

    if (!options.open_with_long_click && !kwargs.sibling){
      this.click_evt_processed = true;
      currentTarget = currentTarget.currentTarget; // Input is Event
    }
    var menu = this.get_element(kwargs.sibling)
		kwargs.preferBottomOffset = 0
		kwargs.expand = {'right': document.getElementById("playlistNext")}
    const reverse = this._prepare(menu, currentTarget, kwargs)

    // Search prev M files
    const M = 15
    const playlist = mpv_status['playlist']
    if (!playlist) return;

    // Range [A,B)
    const B = current_playlist_index(playlist)
    const A = Math.max(0, B-M);
    if (A >= B){
      this._add_info(menu, "No previous entries") // TODO: Did not respect looping
    }else{
      add_entry_args = []
      for(var n=B/*-1*/; n>=A; --n){ // -1 removed to allow jump back to start of current file.
        add_entry_args.push([
          playlist_get_title(playlist[n]),
          function (arg) {
            return function (evt) {
              send("playlist_jump", arg)
              send("play")
            }
          }(n), n+1, [A, B]])
      }

      this._fill_ul(menu, reverse, add_entry_args)
    }
    this.show(menu)

    if (options.show_silbing_menu && !kwargs.sibling){
			kwargs.sibling = true
			kwargs.above = !reverse // Forces position flip for sibling menu because they are too wide for the same vertical position.
      var altTarget = this._getSiblingTarget(currentTarget.id)
      if (altTarget) this.next_files(altTarget, kwargs)
    }
  },

  next_chapters: function show_next_chapters_menu(currentTarget, kwargs){
		kwargs = kwargs || {}

    if (!options.open_with_long_click && !kwargs.sibling){
      this.click_evt_processed = true;
      currentTarget = currentTarget.currentTarget; // Input is Event
    }
    var menu = this.get_element(kwargs.sibling)
		kwargs.preferBottomOffset = 100
		kwargs.expand = {'left': document.getElementById("chapterBack")}
    const reverse = this._prepare(menu, currentTarget, kwargs)

    // Search next M chapters
    const M=100
    const chapters = mpv_status['chapter-list'] || []

    // Note: current chapter index can be -1
    // Range [A,B)
    const A = current_chapter_index(mpv_status)+1
    const B = Math.min(A+M, chapters.length)
    if (A >= chapters.length){
      this._add_info(menu, "No further entries")
    }else{

      add_entry_args = []
      for(var n=A; n<B; ++n){
        add_entry_args.push([
          chapter_get_title(chapters, n),
          function (arg) {
            return function (evt) {
              send("set_chapter", arg)
            }
          }(n), n+1, [A, B]])
      }

      this._fill_ul(menu, reverse, add_entry_args)
    }
    this.show(menu)

    if (options.show_silbing_menu && !kwargs.sibling){
			kwargs.sibling = true
			kwargs.above = !reverse // Forces position flip for sibling menu because they are too wide for the same vertical position.
      var altTarget = this._getSiblingTarget(currentTarget.id)
      if (altTarget) this.prev_chapters(altTarget, kwargs)
    }
  },

  prev_chapters: function show_prev_chapters_menu(currentTarget, kwargs){
		kwargs = kwargs || {}

    if (!options.open_with_long_click && !kwargs.sibling){
      this.click_evt_processed = true;
      currentTarget = currentTarget.currentTarget; // Input is Event
    }
    var menu = this.get_element(kwargs.sibling)
		kwargs.preferBottomOffset = 100
		kwargs.expand = {'right': document.getElementById("chapterForward")}
    const reverse = this._prepare(menu, currentTarget, kwargs)

    // Search next M chapters
    const M=100
    const chapters = mpv_status['chapter-list'] || []

    // Note: Index of chapters shifted by 1. ?!
    // Range [A,B)
    const B = current_chapter_index(mpv_status)
    const A = Math.max(0, B-M);
    if (A >= B){
      this._add_info(menu, "No previous entries")
    }else{
      add_entry_args = []
      for(var n=B/*-1*/; n>=A; --n){ // -1 removed to allow jump back to start of current chapter.
        add_entry_args.push([
          chapter_get_title(chapters, n),
          function (arg) {
            return function (evt) {
              send("set_chapter", arg)
            }
          }(n), n+1, [A, B]])
      }

      this._fill_ul(menu, reverse, add_entry_args)
    }
    this.show(menu)

    if (options.show_silbing_menu && !kwargs.sibling){
			kwargs.sibling = true
			kwargs.above = !reverse // Forces position flip for sibling menu because they are too wide for the same vertical position.
      var altTarget = this._getSiblingTarget(currentTarget.id)
      if (altTarget) this.next_chapters(altTarget, kwargs)
    }
  },

  list_subtitle: function (currentTarget, kwargs){
		kwargs = kwargs || {}

    if (!options.open_with_long_click && !kwargs.sibling){
      this.click_evt_processed = true;
      currentTarget = currentTarget.currentTarget; // Input is Event
    }
    var menu = this.get_element(kwargs.sibling)
		kwargs.preferBottomOffset = 0
    const reverse = this._prepare(menu, currentTarget, kwargs)
    const tracklist = mpv_status['track-list']

    // Loop over track-list and construct titles
    function _text(sub_track){
      if (sub_track.hasOwnProperty('title')) return sub_track.title;
      else if (sub_track.hasOwnProperty('lang')) return sub_track.lang;
      else if (sub_track.hasOwnProperty('codec')) return sub_track.codec;
      return ""
    }

    add_entry_args = []
    for (var i = 0; i < tracklist.length; i++){
      if (tracklist[i].type === 'sub') {

        var idx = tracklist[i].id
        add_entry_args.push([
          _text(tracklist[i]),
          function (arg) {
            return function (evt) {
              send("set_subtitle", arg)
            }
          }(idx),
          idx, [0, window.subs.count]])
      }
    }
    this._fill_ul(menu, reverse, add_entry_args)
    this.show(menu)
  },

  list_video: function (currentTarget, kwargs){
		kwargs = kwargs || {}

    if (!options.open_with_long_click && !kwargs.sibling){
      this.click_evt_processed = true;
      currentTarget = currentTarget.currentTarget; // Input is Event
    }
    var menu = this.get_element(kwargs.sibling)
		kwargs.preferBottomOffset = 0
    const reverse = this._prepare(menu, currentTarget, kwargs)
    const tracklist = mpv_status['track-list']

    // Loop over track-list and construct titles
    function _text(vid_track){
      if (vid_track.hasOwnProperty('title')) return vid_track.title;
      else if (vid_track.hasOwnProperty('demux-w')){
        return `${vid_track['demux-w']}x${vid_track['demux-h']}`;
      }
      else if (vid_track.hasOwnProperty('codec')) return vid_track.codec;
      else if (vid_track.hasOwnProperty('decoder-desc')) return vid_track.decoder-desc;
      else if (vid_track.hasOwnProperty('lang')) return vid_track.lang;
      return ""
    }

    add_entry_args = []
    for (var i = 0; i < tracklist.length; i++){
      if (tracklist[i].type === 'video') {

        var idx = tracklist[i].id
        add_entry_args.push([
          _text(tracklist[i]),
          function (arg) {
            return function (evt) {
              send("set_video", arg)
            }
          }(idx),
          idx, [0, window.subs.count]])
      }
    }
    this._fill_ul(menu, reverse, add_entry_args)
    this.show(menu)
  },

  list_audio: function (currentTarget, kwargs){
		kwargs = kwargs || {}

    if (!options.open_with_long_click && !kwargs.sibling){
      this.click_evt_processed = true;
      currentTarget = currentTarget.currentTarget; // Input is Event
    }
    var menu = this.get_element(kwargs.sibling)
		kwargs.preferBottomOffset = 0
    const reverse = this._prepare(menu, currentTarget, kwargs)
    const tracklist = mpv_status['track-list']

    // Loop over track-list and construct titles
    function _text(audio_track){
      if (audio_track.hasOwnProperty('title')) return audio_track.title;
      else if (audio_track.hasOwnProperty('lang')) return audio_track.lang;
      else if (audio_track.hasOwnProperty('demux-samplerate')) return audio_track.demux-samplerate;
      else if (audio_track.hasOwnProperty('codec')) return audio_track.codec;
      else if (audio_track.hasOwnProperty('decoder-desc')) return audio_track.decoder-desc;
      return ""
    }

    add_entry_args = []
    for (var i = 0; i < tracklist.length; i++){
      if (tracklist[i].type === 'audio') {

        var idx = tracklist[i].id
        add_entry_args.push([
          _text(tracklist[i]),
          function (arg) {
            return function (evt) {
              send("set_subtitle", arg)
              send("play")
            }
          }(idx),
          idx, [0, window.subs.count]])
      }
    }
    this._fill_ul(menu, reverse, add_entry_args)
    this.show(menu)
  },

  seek_menu: function (currentTarget, seconds_list, kwargs){
		kwargs = kwargs || {}

    if (!options.open_with_long_click && !kwargs.sibling){
      this.click_evt_processed = true;
      currentTarget = currentTarget.currentTarget; // Input is Event
    }
    var menu = this.get_element(kwargs.sibling)
		kwargs.preferBottomOffset = 0
    const reverse = this._prepare(menu, currentTarget, kwargs)

    add_entry_args = []
    for (var i = 0; i < seconds_list.length; i++){
      add_entry_args.push([
        format_time2(seconds_list[i]),
        function (arg) {
          return function (evt) {
            send("seek", arg)
          }
        }(seconds_list[i]),
        -1, [0, seconds_list.length]])
    }
    this._fill_ul(menu, reverse, add_entry_args)
    menu.children[0].style.setProperty('text-align', 'center')
    this.show(menu)

    if (options.show_silbing_menu && !kwargs.sibling){
			kwargs.sibling = true
			kwargs.above = reverse // Forces same position for sibling menu
      var altTarget = this._getSiblingTarget(currentTarget.id)
			console.log("HEY" + altTarget)
      if (altTarget) this.seek_menu(altTarget, seconds_list.map((s)=>-s), kwargs)
    }
  },

  _avoid_menu_hide: function(evt) {
    if (!touchmenu.touchstart_in_menu) {
      DEBUG_TOUCH && console.log("Touchstart!")
      touchmenu.touchstart_in_menu = true
    }
  },

  _skip_touchend_handler: function(evt) {
    if (!touchmenu.touchmove_in_menu) {
      DEBUG_TOUCH && console.log("Touchmove!")
      touchmenu.touchmove_in_menu = true
    }
  },

  _menu_handler_initialized: {},
  _resize_handler: {},
  _resize_handler_sibling: {},
  _hidden : true,

  /* FF-Mobile: Save current innerHeight to respect geometry change
   * by hide/show of addressbar by scrolling.
   */
  _menu_innerHeights: {},
  _update_innerHeight: false,
}

function captureEvents(evt) {
  //DEBUG_TOUCH && console.log('Check ' + evt.type)
  if (!touchmenu.options.catch_events_outside) return;
  if (touchmenu._hidden) return;

  if (evt.type == 'touchend' && longpress.done){
    DEBUG_TOUCH && console.log('Longpress ended')
    longpress.done = false

    // Ohne das: Nach Touch außerhalb zweimal klicken notwendig
    // Mit diesem: Menü wird nach Klick geschlossen :-(
    //touchmenu.touchstart_in_menu = false
    return;
  }

  /* Check if we're inside or outside of menu. This flag
   * is set by capturing other events.*/
  if ( !touchmenu.touchstart_in_menu ) {
    DEBUG_TOUCH && console.log('->Catch ' + evt.type)
    evt.stopPropagation()
    if (evt.type == 'click' /*|| evt.type == 'touchend' Doppelt :-(*/){
      if (touchmenu.click_evt_processed){
        touchmenu.click_evt_processed = false;
        longpress.done = false;

      }else{
        hideTouchMenu(evt) // resets touchstart_in_menu
      }
    }
    return
  }
}


function hideTouchMenu(evt) {
  DEBUG_TOUCH && console.log('Hide? ' + evt.type +
    "  |" + touchmenu.touchstart_in_menu + ", " + longpress.done)
  if (touchmenu._hidden) {
    return;
  }

  if( touchmenu.touchstart_in_menu || longpress.done)
  {

    if (touchmenu.touchstart_in_menu){
      //DEBUG_TOUCH && console.log("Abort because of touchstart_in_menu (" + evt.type + ")")
      touchmenu.touchstart_in_menu = false
      touchmenu.touchmove_in_menu = false
    }
    if (longpress.done) { // Skip hide on first click after long touch
      //DEBUG_TOUCH && console.log("Abort because of longpress.done (" + evt.type + ")")
      longpress.done = false
    }

    return;
  }

  //if (touchmenu.longpress.done !== null) {
  if (!touchmenu._hidden) {
    //DEBUG_TOUCH && console.log("Hide touch menu (" + evt.type + ")")
    touchmenu.hide(touchmenu.get_element())
  }
}

/* Avoids firing slider events.
 *
 * Note that the following lines also preventing the change of
 * sliders, but you will still got graphical feedback on the slider.
 * 
 * window.addEventListener('input', captureEvents, {'capture': true})
 * window.addEventListener('change', captureEvents, {'capture': true})
*/
function lock_slider_state(value){
  var sliders = document.getElementsByClassName("slider")
  if (value === true) {
    [].slice.call(sliders).forEach(function (slider) {
      slider.style.setProperty('pointer-events', 'none')
    })
  } else {
    [].slice.call(sliders).forEach(function (slider) {
      slider.style.removeProperty('pointer-events')
    })
  }
}


// both, click and mouseup needed because evt.stopPropagation() do not stop the other one
window.addEventListener('click', captureEvents, {'capture': true})
window.addEventListener('mouseup', captureEvents, {'capture': true})
window.addEventListener('pointerup', captureEvents, {'capture': true})
window.addEventListener('touchend', captureEvents, {'capture': true})
/*window.addEventListener('input', captureEvents, {'capture': true})
window.addEventListener('change', captureEvents, {'capture': true})*/

// Capture of pointerup/mouseup/touchend in bubble phase is problematic 
// because elements of touchmenu could be handled by 'click' later.
/*window.addEventListener('mouseup', hideTouchMenu, {'capture': false})
*/
window.addEventListener('click', hideTouchMenu, {'capture': false})
//window.addEventListener('touchend', hideTouchMenu, {'capture': false})

//TODO
screen.orientation.addEventListener('change', (evt) => {
  //console.log("Orient!" + screen.orientation.type)

  // Note that window.innerHeight is not updated to value after orientation
  // change (2022). We need to add a one-time listener to the next resize event.
  // or…
  /*
   * window.addEventListener('resize', function(evt) {
   *   // Reset some anchors to new innerHeight.
   *   Object.keys(touchmenu._menu_innerHeights).forEach( (m) => {
   *     touchmenu._menu_innerHeights[m] = window.innerHeight
   *   })
   * }, {once:true, capture:true});
   */
  // … setting a flag forresize-handler in touchmenu._prepare.
  // This is the preferred variant because above listener wouldn't fire before
  // the other handler, but after.
  touchmenu._update_innerHeight = true

})

