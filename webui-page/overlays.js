var overlays = {}

function hide_overlays(){
  for (id in overlays){
    var el = document.getElementById(id)
    if (el && el.style.visibility === "visible") overlays[id](false)
  }
}

function toggleOverlay(id, force) {

  let el = document.getElementById(id)
  let overlay_is_visible_new = force ||
    el.style.getPropertyValue("visibility") !== "visible"

  document.body.classList.toggle('noscroll', overlay_is_visible_new)
  el.style.setProperty("visibility", (overlay_is_visible_new ? "visible" : "hidden"))

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
