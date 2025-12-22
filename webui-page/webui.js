/* Note about code structure:
 *  The code is splitted into multiple files but not into modules. The will
 *  be threaten as monolitic block.
 *
 */

// Variables in global context
const DEBUG = true,
    DEBUG_MOBILE = false,
    DEBUG_TOUCH = true,
    USE_DUMMY_AUDIO_ELEMENT = false, /* for controls on lock screen. */
    USE_SWIPES = true

const UNIT_GAP = " "

const REGEXS = {
  'brackets': new RegExp('\\[[^\\]]*\\]', 'g'), // [...]
  'extension': new RegExp('[.][A-z]{3,10}$', ''),   // .ext
  'checksumBeforeExtension': new RegExp('-[A-z0-9-]{10,12}([.][A-z]+)$', ''), // -AbCdEXFGL.
  'cleanup_dots': new RegExp('[.…]*…[.…]*','g'),
}

let ws = null
let mpv_status = {}

/* Collect [num] milliseconds status updates before
 * they will be used to update the page. This is useful to
 * avoid too many updates for fast changing variables.
 */
let mpv_outstanding_status = {
  delay: 300,
  updates: {},
  timer: null,
  pending: false
}

// Variables in window context
var metadata = {},
    subs = {},
    audios = {},
    videos = {},
    connected = false,
    max_title_size = 60,
    tab_in_background = false,
    prevActiveElement = null


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

