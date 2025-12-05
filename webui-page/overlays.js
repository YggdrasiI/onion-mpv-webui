var overlays = {}
var id_prev = undefined  // Last active overlay

function hide_overlays(){
  for (id in overlays){
    var el = document.getElementById(id)
    if (el && el.style.visibility === "visible") overlays[id](false)
  }
}

function show_overlay(id) {
	overlays[id](true)
}

function hide_overlay(id) {
	overlays[id](false)
}

function show_next_overlay(){
	/* Assuming that just zero or one overlay is visible */
	let id_next = undefined
	let id_cur = undefined
  for (id in overlays){
		if (id_next === undefined) id_next = id  // First element is fallback
		if (id_cur !== undefined) { 
			id_next = id
			break;
		}
    let el = document.getElementById(id)
    if (el && el.style.visibility === "visible"){
			id_cur = id // to break in next loop step
		}
  }

	// Use last open id instead of first overlay if none is open
	if (id_cur === undefined && id_prev != undefined){
		id_next = id_prev
	}

	// Hide old and show new
	if (id_cur) hide_overlay(id_cur)
	if (id_next) show_overlay(id_next)
}

function toggleOverlay(id, force) {

  let el = document.getElementById(id)
  let overlay_is_visible_new = force ||
    el.style.getPropertyValue("visibility") !== "visible"

  document.body.classList.toggle('noscroll', overlay_is_visible_new)
  el.style.setProperty("visibility", (overlay_is_visible_new ? "visible" : "hidden"))

	if (!overlay_is_visible_new) id_prev = id

  /* Close overlay by click on background area. */
  if (!el.hasOwnProperty("close_event_registered") ){
    function _click_on_background(target, overlay){
      if ( target == overlay ||
        target.parentElement == overlay && target.childElementCount == 0 )
        return true
      return false
    }

    function addBackupActiveElement(overlay){
      Array.from(overlay.getElementsByTagName("input")).forEach((el) => 
        el.addEventListener("focusout", function(evt) {document.prevActiveElement = evt.target}))
    }

    function _close_listener(evt) {
      evt.stopPropagation();

      // 1. Check abort criteria
      // 1.1) Abort if select/input looses focus by click
      let prevActiveElement = document.prevActiveElement;
      document.prevActiveElement = null // reset
      if (document.activeElement.localName === "select"
        // || document.activeElement.localName === "input" /* This does not work because it's already 'body' active */
        || prevActiveElement /* set by focusout-Event */){
        DEBUG && console.log("Skip hide because of focused input element.")
        return
      }

      // 1.2) Abort if not clicked on empty region of overlay*/
      if (!_click_on_background(evt.target, el)) return

      toggleOverlay(id, false)
    }
    el.addEventListener('click', _close_listener, false)
    el.close_event_registered = true

    addBackupActiveElement(el) // track focusout events for input's

    // Swipe gestures
    USE_SWIPES && add_overlay_swipes(id)
  }

  return overlay_is_visible_new
}
