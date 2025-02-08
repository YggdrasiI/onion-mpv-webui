const DEBUG_TOUCH = false

const options = {
  swap_short_and_long_press: true,     /* If true, menu will open with sort press/click.
                                  * If true, button will triggered by short click
                                  * and menu will be open by longpress.
                                  */
  update_default_entry: false,   /* Selected menu entry will replace default 
                                  * entry for short press. (NOT IMPLEMENTED)
                                  */
  default_entry_in_menu: false,  /* If false, the shortpress entry is not included 
                                  * in touchmenu list. (NOT IMPLEMENTED)
                                  */
  show_silbing_menu: true,       /* If true the menu for the 'opposite operation' will also be shown. */
}

/* If 'show_silbing_menu' is true this list decides
 * which menu(s) will shown, too.*/
const sibling_ids = {
//  playlistPrev: ["playlistNext"],
//  playlistNext: ["playlistPrev"],
  seekBack1: ["seekForward1"],
  seekForward1: ["seekBack1"],
//  chapterBack: ["chapterForward"],
//  chapterForward: ["chapterBack"],
}


/* Helper object to create elements with short and long press events */
var longpress = {
  options : {
    ms: 400,  // Length for a long press on buttons
  },
  done: null, /* Tri state flag to avoid firing both of shortpress_h/longpress_h.
                  • true: Last click/touch was a long one.
                          => Do not fire 'shortpress_h' on next mouseup/touchend event.
                  • false: Last click/touch was short or already an other
                          mouseup/touchend event occoured.
                          => Do not fire 'longpress_h' after timeout.
                  • null: State before everything started and after both events finished.
                  */
  timer: null,

  _abortLongpress: function (){
    if (longpress.timer) {
      clearTimeout(longpress.timer);
      longpress.timer = null
    }
  },

  addEventHandler: function(els, evt_type, shortpress_h, longpress_h){
    if (els.length == 0) return;

    // Threat undefined handlers like null
    if (shortpress_h === undefined) shortpress_h = null
    if (longpress_h === undefined) longpress_h = null

    if (options.swap_short_and_long_press && longpress_h !== null){
      [shortpress_h, longpress_h] = [longpress_h, shortpress_h]
    }

    if (longpress_h !== null && !evt_type in ['mouseup', 'pointerup']){
      console.log("Warning, touchmenu defined for event != mouseup/pointerup."
      + "Element: " + (els[0].getAttribute('id')||els[0].getAttribute('name')) )
    }

    var fshort = function (evt) {

      if (!touchmenu._hidden){
        DEBUG_TOUCH && console.log("Menu is open… Skip shortpress event")
        return;
      }

      if (longpress.done === true){ // longpress handler had already fired.
        longpress.done = null // Reset
        return
      }

      longpress._abortLongpress()
      longpress.done = false
      shortpress_h(evt)
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
      let flong = function (evt) {
        if (!touchmenu._hidden){
          DEBUG_TOUCH && console.log("Menu is open… dont start longpress timer.")
          return;
        }
        // Reset tri-state (false -> short fired, true -> long fired.
        longpress.done = null // Probably redundant

        longpress.timer = setTimeout(
          function(target) {
            DEBUG_TOUCH && console.log("Longpress START " + target.constructor.name)
            return function() {
              longpress.timer = null
              if (longpress.done === false){ // shortpress handler had already fired.
                longpress.done = null
                return
              }
              DEBUG_TOUCH && console.log("Longpress END " + target.constructor.name)
              longpress.done = true
              let longpressEvt = new CustomEvent("longpress", {
                // Add other event properties used by your handlers, here.
              })
              target.dispatchEvent(longpressEvt)
            }
          }(evt.currentTarget), longpress.options.ms);
      }

      let flongAbort = function (evt) {
        longpress._abortLongpress()
      }

      els.forEach(el => {
        el.addEventListener('longpress', longpress_h)

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
    id_prefix: 'touchmenu',  // -> id_prefix{number}
  },

  /* Flag set on pointerdown/mousedown/touchstart.
   * Used for 
   *    • Avoid observeEventsForTouchmenu if event occoured inside of menu.
   *    • Abort menu hide after touchend.
   */
  _touchstart_in_menu: false, 


  /* To abort touchend events of menu after scrolling its elements.
   */
  _touchmove_in_menu: false, // TODO unify with _touchstart_in_menu ?!

  _menu_handler_initialized: {},
  _resize_handler: {},
  _hidden : true,
  _recently_open : false,
  _max_number_open_menus : 0,
  _menu_ids : [],

  _avoid_menu_hide: function(evt) {
    if (!touchmenu._touchstart_in_menu) {
      DEBUG_TOUCH && console.log("Pointerdown/Touchstart!")
      touchmenu._touchstart_in_menu = true
    }
  },

  _skip_touchend_handler: function(evt) {
    if (!touchmenu._touchmove_in_menu) {
      DEBUG_TOUCH && console.log("Touchmove!")
      touchmenu._touchmove_in_menu = true
    }
  },

  /* FF-Mobile: Save current innerHeight to respect geometry change
   * by hide/show of addressbar by scrolling.
   */
  _menu_innerHeights: {},
  _update_innerHeight: false,

  _gen_menu_elements: function(){
    let menu_ids = []
    let max_number_open_menus = 0
    if (!options.show_silbing_menu){
      max_number_open_menus = 1
      menu_ids.push(`${this.options.id_prefix}0`)
    }else{
      // Maximal array length in sibling_ids-values:
      max_number_open_menus = Object.values(sibling_ids).reduce(
        (n, value) => Math.max(n, value.length), 0)
      // Entry for id=0 and max_id siblings.
      for (let i = 0; i <= max_number_open_menus; i++) menu_ids.push(`${this.options.id_prefix}${i}`)
    }

    DEBUG_TOUCH && console.log(`Create ${menu_ids.length} elements for touchmenus`)
    ;[].slice.call(menu_ids).forEach(function (menu_id) {
      menu = document.createElement('div')
      menu.id = menu_id
      menu.classList.add("touchmenu");
      menu.style.cssText = `
        position: absolute;
        display:none;
        `;

      document.getElementsByTagName('body')[0].appendChild(menu)
    })

    this._max_number_open_menus = max_number_open_menus
    this._menu_ids = menu_ids
  },

  _getSiblingTargets: function(primary_id){
    var alt_ids = sibling_ids[primary_id]
    if (alt_ids) return alt_ids.map((id) => document.getElementById(id))
    return []
  },

  _get_element: function(sibling_id){
    if (this._menu_ids.length === 0) this._gen_menu_elements()

    if (sibling_id){
      //return this._menu_ids.slice(1).map((id) => document.getElementById(id))
      return document.getElementById(this._menu_ids[sibling_id])
    }else{
      return document.getElementById(this._menu_ids[0])
    }
  },

  show: function (menu) {
    this._recently_open = true
    this._hidden = false
    lock_slider_state(true)
    if (!menu) {
      menu = this._get_element()
    }
    menu.style.setProperty('display', 'block')
  },

  hide: function(menu){
    // Hide just explicit given element
    if (menu) {
      menu.style.setProperty('display', 'none')
      return
    }

    if (this._hidden) return;
    this._hidden = true
    lock_slider_state(false)

    // Hide all menus
    if (options.show_silbing_menu){
      this._menu_ids.map((id) => document.getElementById(id)
      ).forEach((el) => el.style.setProperty('display', 'none'))
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

    // For _position in resize handler defined below
    kwargs.during_resize = kwargs.during_resize || false

    if (this._menu_handler_initialized.hasOwnProperty(menu.id) === false){
      DEBUG_TOUCH && console.log("Add menu listener")
      //menu.addEventListener('mousedown', this._avoid_menu_hide)
      //menu.addEventListener('touchstart', this._avoid_menu_hide)
      menu.addEventListener('pointerdown', this._avoid_menu_hide)
      menu.addEventListener('touchmove', this._skip_touchend_handler)
      this._menu_handler_initialized[menu.id] = true
    }
    this._menu_innerHeights[menu] = window.innerHeight

    // Reset flags
    this._touchstart_in_menu = false
    this._touchmove_in_menu = false

    // Clear old content
    menu.style.setProperty('display', 'none')
    menu.replaceChildren()

    // Eval new position. Here the above flag is undefined, but defined in all later calls. We fixing this value because otherwise the order of elements needs to be reversed.
    var above = this._position(menu, currentTarget, kwargs)

    // Define new resize handler (respecting arguments of this function)
    // and then replace previous one
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

    // Remove previous resize handler(s)
    if (!kwargs.sibling){
      Object.values(this._resize_handler).forEach((h) =>
        window.removeEventListener('resize', h))
      this._resize_handler = {}
    }

    // Save handler for next remove
    this._resize_handler[menu.id] = new_handler

    // Add resize handler
    window.addEventListener('resize', this._resize_handler[menu.id])

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
    }else{
      var above = false
      menu.style.setProperty('top', Math.round(
        rect.bottom + window.scrollY + addressbar_offset
      )+'px')
      menu.style.removeProperty('bottom')
    }
    return above
  },

  _add_entry: function (ul, title, handler, idx, playlist_range) {
    var el = document.createElement('li')
    //el.classList.add("button", "content", "playlist-controls", "touch-entry")
    el.classList.add("touchmenu-entry")

    if (idx<0) {
      el.innerText = title
    }else{
      var s1 = document.createElement('span')
      s1.classList.add("index")
      var s2 = document.createElement('span')
      s1.innerText = `#${idx}`
      s2.innerText = title
      el.appendChild(s1)
      el.appendChild(s2)
    }

    el.addEventListener("click", handler)
    ul.appendChild(el)
  },

  _add_info: function (menu, text) {
    var el = document.createElement('li')
    el.classList.add("touch-info")
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

  _target(evt, kwargs){
    let target = evt.currentTarget
    if (kwargs.sibling){ // Use other button as anchor
      target = this._getSiblingTargets(target.id)[kwargs.sibling-1]
    }
    return target
  },

  next_files: function show_next_files_menu(evt, kwargs){
    kwargs = kwargs || {}

    let currentTarget = this._target(evt, kwargs)
    if (!currentTarget) return

    var menu = this._get_element(kwargs.sibling)
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
      kwargs.sibling = 1
      kwargs.above = !reverse // Forces position flip for sibling menu because they are too wide for the same vertical position.
      this.prev_files(evt, kwargs)
    }
  },

  prev_files: function (evt, kwargs){
    kwargs = kwargs || {}

    let currentTarget = this._target(evt, kwargs)
    if (!currentTarget) return

    var menu = this._get_element(kwargs.sibling)
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
      kwargs.sibling = 1
      kwargs.above = !reverse // Forces position flip for sibling menu because they are too wide for the same vertical position.
      this.next_files(evt, kwargs)
    }
  },

  next_chapters: function show_next_chapters_menu(evt, kwargs){
    kwargs = kwargs || {}

    let currentTarget = this._target(evt, kwargs)
    if (!currentTarget) return

    var menu = this._get_element(kwargs.sibling)
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
      kwargs.sibling = 1
      kwargs.above = !reverse // Forces position flip for sibling menu because they are too wide for the same vertical position.
      this.prev_chapters(evt, kwargs)
    }
  },

  prev_chapters: function show_prev_chapters_menu(evt, kwargs){
    kwargs = kwargs || {}

    let currentTarget = this._target(evt, kwargs)
    if (!currentTarget) return

    var menu = this._get_element(kwargs.sibling)
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
      kwargs.sibling = 1
      kwargs.above = !reverse // Forces position flip for sibling menu because they are too wide for the same vertical position.
      this.next_chapters(evt, kwargs)
    }
  },

  list_subtitle: function (evt, kwargs){
    kwargs = kwargs || {}

    let currentTarget = this._target(evt, kwargs)
    if (!currentTarget) return

    var menu = this._get_element(kwargs.sibling)
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

    if (options.show_silbing_menu && !kwargs.sibling){
      kwargs.sibling = 1
      this.list_video(evt, kwargs)
      kwargs.sibling = 2
      this.list_audio(evt, kwargs)
    }
  },

  list_video: function (evt, kwargs){
    kwargs = kwargs || {}

    let currentTarget = this._target(evt, kwargs)
    if (!currentTarget) return

    var menu = this._get_element(kwargs.sibling)
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

    if (options.show_silbing_menu && !kwargs.sibling){
      kwargs.sibling = 1
      this.list_audio(evt, kwargs)
      kwargs.sibling = 2
      this.list_subtitle(evt, kwargs)
    }
  },

  list_audio: function (evt, kwargs){
    kwargs = kwargs || {}

    let currentTarget = this._target(evt, kwargs)
    if (!currentTarget) return

    var menu = this._get_element(kwargs.sibling)
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

    if (options.show_silbing_menu && !kwargs.sibling){
      kwargs.sibling = 1
      this.list_subtitle(evt, kwargs)
      kwargs.sibling = 2
      this.list_video(evt, kwargs)
    }
  },

  seek_menu: function (evt, seconds_list, kwargs){
    kwargs = kwargs || {}

    let currentTarget = this._target(evt, kwargs)
    if (!currentTarget) return

    var menu = this._get_element(kwargs.sibling)
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
      kwargs.sibling = 1
      kwargs.above = reverse // Forces same position for sibling menu
      this.seek_menu(evt, seconds_list.map((s)=>-s), kwargs)
    }
  },

}

/*
 * Checks if menu is open and prevent hitting of buttons, etc outside of menu.
 *
 */
function observeEventsForTouchmenu(evt) {
  DEBUG_TOUCH && console.log('Check ' + evt.type)

  /*
  if (evt.type == 'touchend' && longpress.done){
    DEBUG_TOUCH && console.log('Longpress ended')
    longpress.done = null

    // Ohne das: Nach Touch außerhalb zweimal klicken notwendig
    // Mit diesem: Menü wird nach Klick geschlossen :-(
    //touchmenu._touchstart_in_menu = false
    return;
  }
  */

  if (!touchmenu.options.catch_events_outside) return;
  if (touchmenu._hidden) return;

  if (touchmenu._touchstart_in_menu) return;

  if (touchmenu._recently_open){
    /* Events on other child elements showed touchmenu.
     * So don't hide it immideatly. */
    DEBUG_TOUCH && console.log(`Touchmenu recently open. Avoid menu hide…`)

    evt.stopPropagation()

    /* Reset flag in last event(s) of group (pointerup,mouseup,click)
     * TODO: Touch event switch
     */
    if (evt.type == 'click') {
      touchmenu._recently_open = false
    }
    return;
  }

  /* Here, the menu is open, but no touch/click was started in menu
   * Thus, I can surpress the event, and close the menu. */
  evt.stopPropagation()

  if (evt.type == 'click' /*|| evt.type == 'touchend' Doppelt :-(*/){
    DEBUG_TOUCH && console.log(`Hide touchmenu`)
    //hideTouchMenu(evt) // resets _touchstart_in_menu
    touchmenu.hide()
  }
}


function hideTouchMenu(evt) {
  DEBUG_TOUCH && console.log('Hide? ' + evt.type +
    "  |" + touchmenu._touchstart_in_menu + ", " + longpress.done)
  if (touchmenu._hidden) {
    return;
  }

  if (touchmenu._touchstart_in_menu)
  {
    DEBUG_TOUCH && console.log("Abort because of _touchstart_in_menu (" + evt.type + ")")
    touchmenu._touchstart_in_menu = false
    touchmenu._touchmove_in_menu = false

    return;
  }

  DEBUG_TOUCH && console.log("Hide touch menu (" + evt.type + ")")
  touchmenu.hide()
}

/* Avoids firing slider events.
 *
 * Note that the following lines also preventing the change of
 * sliders, but you will still got graphical feedback on the slider.
 * 
 * window.addEventListener('input', observeEventsForTouchmenu, {'capture': true})
 * window.addEventListener('change', observeEventsForTouchmenu, {'capture': true})
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


/* A) Triggered before event was processed in child elements: ?!*/
// both, click and mouseup needed because evt.stopPropagation() do not stop the other one
window.addEventListener('click', observeEventsForTouchmenu, {'capture': true})
window.addEventListener('mouseup', observeEventsForTouchmenu, {'capture': true})
window.addEventListener('pointerup', observeEventsForTouchmenu, {'capture': true})
window.addEventListener('touchend', observeEventsForTouchmenu, {'capture': true})
/*window.addEventListener('input', observeEventsForTouchmenu, {'capture': true})
window.addEventListener('change', observeEventsForTouchmenu, {'capture': true})*/

/* B) Triggered after event was processed in child elements: */
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

