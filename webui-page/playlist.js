
function togglePlaylist(force) {
  return toggleOverlay("overlay1", force)
}

function createPlaylistTable(entry, position, pause, first) {
  function setActive(set) {
    if (set === true) {
      td_left.innerHTML = '<i class="fas fa-play"></i>'
      td_left.classList.add('active')
      td_2.classList.add('active')
    } else {
      td_left.replaceChildren()
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
    var title = basename(entry.filename)
    // Parts of URLs needs decoding here.
    title = decodeURIComponent(title)
  }

  // limit length of entry
  var title_trimmed = trim_title_string(title, max_title_size)

  var table = document.createElement('table')
  var tr = document.createElement('tr')
  var td_left = document.createElement('td')
  var td_2 = document.createElement('td')
  var td_right = document.createElement('td')
  table.className = 'playlist'
  table.playlist_id = position // Read by some function handler
  tr.className = 'playlist'
  td_2.className = 'playlist'
  td_left.className = 'playlist'
  td_right.className = 'playlist'
  td_2.innerText = title_trimmed
  td_2.setAttribute('title', title)

  var td_3 = document.createElement('td')
  td_3.innerHTML = '<i class="fas fa-arrow-up"></i>'
  td_3.className = 'playlist'

  td_right.innerHTML = '<i class="fas fa-trash"></i>'

  if (entry.hasOwnProperty('playing')) {
    if (pause === "yes") {
      td_left.innerHTML = '<i class="fas fa-pause"></i>'
    } else {
      td_left.innerHTML = '<i class="fas fa-play"></i>'
    }

    td_left.classList.add('playing', 'violet')
    td_2.classList.add('playing', 'violet')
    td_3.classList.add('violet')
    td_right.classList.add('violet')

    td_left.addEventListener('click', () => { togglePlayPause() })

  } else {
    td_left.classList.add('gray')
    td_2.classList.add('gray')
    td_3.classList.add('gray')
    td_right.classList.add('gray')

    function playlist_jump(table) {
      let arg = table.playlist_id
      send("playlist_jump", arg)
      send("play")  // remove pause
      // evt.stopPropagation()
    }

    td_left.addEventListener('click', () => { playlist_jump(table) })
    //td_2.addEventListener('click', () => { playlist_jump(table) })

    /* Switching from mouseover/mouseout to mouseenter/mouseleave because
     * first pair also triggers if mouse in on child elements.
     */
    td_left.addEventListener("mouseenter", function() {setActive(true)})
    td_left.addEventListener("mouseleave", function() {setActive(false)})

    td_left.addEventListener("click", blink)
    //td_2.addEventListener("click", blink)
  }

  td_3.addEventListener('click',
    function (table) {
      return function () {
        let arg = table.playlist_id
        send("playlist_move_up", arg)
      }
    }(table))

  td_right.addEventListener('click',
    function(table) {
      return function() {
        let arg = table.playlist_id
        send("playlist_remove", arg)
      }
    }(table)
  )

  tr.appendChild(td_left)
  tr.appendChild(td_2)
  tr.appendChild(td_3)
  tr.appendChild(td_right)
  table.appendChild(tr)
  return table
}

function populatePlaylist(json, pause) {
  var playlist = document.getElementById('playlist')
  playlist.replaceChildren()

  var first = true
  var playedRow = null
  for(let i = 0; i < json.length; ++i) {
    let row = createPlaylistTable(json[i], i, pause, first)
    playlist.appendChild(row)
    if (first === true) {
      first = false
    }

    if (json[i].hasOwnProperty('playing')) {
      playedRow = row;
    }
  }

  // Scroll played entry into view
  if (playedRow) playedRow.scrollIntoView({block: 'center'})

}

function updatePlaylistOnPause(current_playlist, new_pause, old_pause) {
  if (new_pause == old_pause) return;
  var playlist = document.getElementById('playlist')

  for (let i=0; i<current_playlist.length; ++i){
    if (current_playlist[i].current && current_playlist[i].current == true){

      let row = createPlaylistTable(current_playlist[i], i, new_pause, (i==0))
      playlist.replaceChild(row, playlist.children[i])
      break;
    }
  }
}

function updatePlaylist(new_playlist, old_playlist, new_pause) {
  /* Compare both lists and just update changed rows.
   * It works well if just one element is removed/inserted
   * or some Elements are swapped.
   *
   * Inserts of multiple elements can not be detected and
   * will handled like a complete new (part of a) list.
   */

  var compare_handler = function(new_entry, old_entry){
    if (new_entry.filename == old_entry.filename
      && (new_entry.current == old_entry.current))
      return true;
    return false
  }

  var operations = get_diff(new_playlist, old_playlist, compare_handler)
  //DEBUG && console.log(operations)

  var playlist = document.getElementById('playlist')
  var overlay = playlist.parentElement
  // children Index of playlist matches with above index.

  var offset = 0
  operations.forEach( x => {
    if (x['op'] == 'replace' /* || x['op'] == 'changed'*/) {
      var row = createPlaylistTable(
        new_playlist[x['new']],
        x['old'], new_pause, (x['new']==0))
      playlist.replaceChild( row,
        playlist.children[x['old'] + offset])
    }else if (x['op'] == 'add') {
      var row = createPlaylistTable(
        new_playlist[x['new']],
        x['old'], new_pause, (x['new']==0))
      playlist.insertBefore( row,
        playlist.children[x['old'] + offset])//.nextElementSilbing)
      offset++
    }else if (x['op'] == 'remove') {
      playlist.removeChild(
        playlist.children[x['old'] + offset])
      offset--
    }
  })

  // Update playlist indizes for event handlers.
  var id = 0;
  playlist.childNodes.forEach(n => { n.playlist_id = id++; })

  /* Scroll played entry into view, if menu is hidden.
   * Scrolling if open would be anoying...
   * Maybe a button to scroll to active entry would be nice
   * to catch the open playlist case.
   */
  if (overlay.style.getPropertyValue("visibility") === "hidden"){
    var pl_index = current_playlist_index(new_playlist)
    if (pl_index >= 0) {
      playlist.children[pl_index].scrollIntoView({block: 'center'})
    }
  }

}
