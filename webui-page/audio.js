/* Functions to play locally 'dummy audio file' to get
 * media controls on smartphone lock screens. */
function audioLoad() {
  if (!USE_DUMMY_AUDIO_ELEMENT) return;

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

function audioPlay() {
  if (!USE_DUMMY_AUDIO_ELEMENT) return;

  var audio = document.getElementById("audio")
  if (audio.paused) {
    block.doublePause = true
    audio.play()
      .then(_ => { __mediaSession()
        DEBUG && console.log('Playing dummy audio'); })
      .catch(error => { DEBUG && console.log(error) })
    navigator.mediaSession.playbackState = "playing"
    // (Almost redundant line. Value will be overwritten with feedback of mpv)
  }
}

function audioPause() {
  if (!USE_DUMMY_AUDIO_ELEMENT) return;

  var audio = document.getElementById("audio")
  if (!audio.paused) {
    block.doublePause = true;
    DEBUG && console.log('Pausing dummy audio')
    audio.pause()
    navigator.mediaSession.playbackState = "paused"
    // (Almost redundant line. Value will be overwritten with feedback of mpv)
  }
}

window.addEventListener('load', audioLoad, false)
