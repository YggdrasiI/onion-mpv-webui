
// TODO: Is it possible to use this as static function of KeyBindings class?!
function unfold_bindings(bindings){
  // 1. Split into two sub-arrays by the shift-flag:
  // The resulting object has the keys "true" and "false"…
  const split_by_modifier_shift = bindings.reduce((result, obj) => {
    const key = (obj.shift?true:false) // merges false and undefined
    result[key] = result[key] || []
    result[key].push(obj)
    return result;
  }, {})

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
    {
      "key": "Escape",
      "code": 27,
      "handle": function(evt) { hide_overlays(); }
    },
    {
      "key": "Enter",
      "code": 13,
      "handle": function(evt) { show_next_overlay(); }
    },
    {
      "key": "m",
      "code": 58,
      "command": "toggle_mute",
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
  if (evt.key == "Unidentified"){
    /* FF Mobile returns keyCode == 0 und evt.key == "Unidentified" for press on Finger sensor ?! This if statement prevents triggering of '0' command (add_volume)…
     * TODO: Replace this check by some more robust/verbose variant, e.g. checking some event type which differs?!*/
    return
  }

  let binding = KeyBindings.for_event(evt) // null or Object assigned to this key+modifier combination.
  if (binding === null) return

  if (document.activeElement.localName === "input" /* localName == tagName.toLowerCase() */
    ||document.activeElement.localName === "select")
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

  if (binding.handle){
    binding.handle(evt)
  }

  if (binding.command){
    send(binding.command, binding.param1, binding.param2)
  }

  if (binding.handle || binding.command){
    evt.stopPropagation()
    evt.preventDefault()  // e.g. avoids 'scrolling by space key'
  }
}

