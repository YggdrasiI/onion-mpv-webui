
const DEBUG_SORTINGS = false

var sortings = {
  "alpha": {
    // id for direction, normally just 1 for forward and -1 for backward
    "dirs": [-1, 1, 2, -2],
    "icons": ["fa-sort-alpha-up", "fa-sort-alpha-down", "fa-folder-open", "fa-folder-open"],
    "active": 0, //index of above lists
  },
  "date": {
    "dirs": [-1, 1],
    "icons": ["fa-sort-numeric-up", "fa-sort-numeric-down"],
    "active": 0 //index of above lists
  }
}

var shares = {
  list: null,
  sorting: null /* Using input lists unsorted */, //{'sname':'alpha'},
  selected: null,
  close_event_registered: false,
  scroll_positions: {},
  local_sorting: {}
}

function toggleShares(force) {
  let overlay_is_visible = toggleOverlay("overlay2", force)

  if (overlay_is_visible){
    if (shares.list == null){
      refresh_share_list()
    }
  }
  return overlay_is_visible
}

function browseShares() {
  /* Open current folder in new tab */
  var url = basename(shares.list[shares.selected].url);
  var dir = shares.list[shares.selected].dir
  //var local_path = encodeURIComponent(`${url}/${dir}`)
  var local_path = `${url}/${dir}`
  var cur_dir = `/media/html/${local_path}`
  cur_dir = cur_dir.replace(/[/]+$/gm,"")

  true | DEBUG && console.log(`Open ${cur_dir}`)
  window.open(cur_dir, window.metadata.title).focus()
}

function share_change(el){
  /* Save current scroll position */
  var sharelist = document.getElementById("sharelist")
  var prev_dirname = shares.scroll_positions["active_dirpath"]
  if( prev_dirname !== undefined ){
    var sT = sharelist.scrollTop;
    DEBUG && console.log("Save scroll position of share " +
      prev_dirname + " to " + sT)
    shares.scroll_positions[prev_dirname] = sT

    // Backup current sorting and its direction
    if (shares.sorting) {
      var sname = shares.sorting.sname
      DEBUG_SORTINGS && console.log(`Save sorting ${shares.sorting.sname} ${sortings[sname].active}`)
      shares.local_sorting[prev_dirname] = {
        "sname": shares.sorting.sname,
        "active": sortings[sname].active
      }
    }
  }

  var iSel = el.selectedIndex
  var request = new XMLHttpRequest()
  if (iSel == -1){
    request.open("get", "/media/api/list/.current/.current")
  }else{
    //var value = el.options[el.selectedIndex].value
    //var text = el.options[el.selectedIndex].text
    var s = shares.list[el.selectedIndex]
    request.open("get", s["url"] + "/" + s["dir"])
    //request.open("get", s["url"] + "/" + encodeURIComponent(s["dir"]) )
  }

  request.onreadystatechange = function() {
    if (request.readyState === 4 && request.status === 200) {
      var json = JSON.parse(request.responseText)
      update_selected_share(json);
      print_share_list(json)
    } else if (request.status === 0) {
      DEBUG && console.log("Fetching share list failed")
    }
  }
  request.send(null)
}

function update_selected_share(json){
  //console.log(json)
  shares.selected = -1
  for(let i = 0; i < shares.list.length; ++i) {
    var full_dirpath = `${json.commands.list}/${json.dirpath}`
    console.log(`${full_dirpath} vs ${shares.list[i].url}`)
    if (full_dirpath.startsWith(shares.list[i].url)){
      shares.selected = i
      document.getElementById("share_selector").options.selectedIndex = i
      DEBUG && console.log(`Selecting share ${i} (${shares.list[i].url})`)
      break
    }
  }
  if (shares.selected == -1){
    console.log("Error. Given path is no child directory of a share.")
    DEBUG && console.log(json)
    shares.selected = 0

    if (shares.list.length == 0) return;
  }

  if (shares.selected && shares.list[shares.selected].dir === ".current"){
    DEBUG && console.log(`Replace .current by '${share_get_subdir(json.dirpath)}'`)
    shares.list[shares.selected].dir = share_get_subdir(json.dirpath)
  }

  //shares.selected = el.selectedIndex
}

function share_change_dir(list_link){
  DEBUG && console.log(`Selected share index: '${shares.selected}'`)
  if (list_link === ".") {
    let cur_dir = shares.list[shares.selected].url + "/"
      + shares.list[shares.selected].dir
    list_link = cur_dir
  }
  if (list_link === "..") {
    let cur_dir = shares.list[shares.selected].url + "/"
      + shares.list[shares.selected].dir
      .replace(/[/]+$/gm,""); // remove '/' at end

    let last_token = basename(cur_dir)
    let parent_dir = cur_dir.substring(0, cur_dir.length - last_token.length)
    list_link = parent_dir
  }
  if (list_link === "/") {
    list_link  = shares.list[shares.selected].url + "/"
  }
  DEBUG && console.log(`Link of new share dir: '${list_link}'`)

  let new_dir = share_get_subdir(list_link);
  DEBUG && console.log(`Relative dir of new share dir: '${new_dir}'`)

  shares.list[shares.selected].dir = new_dir
}

function share_get_subdir(dirname){
  /* strips prefix /media/api/list/<sharename> from dirname
   * and remove leading and trailing '/'.
   */

  var root = shares.list[shares.selected].url
  if (!dirname.startsWith(root)) return ""

  var relative_dir = dirname.replace(root, "")
    .replace(/^[/]+|[/]+$/gm,'');  // Remove '/' at begin and end

  return relative_dir
}

function share_add_file(add_link, bPlay){
  if (add_link === "..") {
    DEBUG && console.log("'..'-dirs cannot be added.")
    return
  }

  //var url = shares.list[shares.selected].url + link
  DEBUG && console.log("Add file: " + add_link)

  if ( bPlay ){
    add_link = add_link.replace("playlist_add","playlist_add/replace")
  }else{
    add_link = add_link.replace("playlist_add","playlist_add/append")
  }

  var request = new XMLHttpRequest();
  //request.open("get", add_link)
  //add_link = preserve_special_chars(add_link)
  request.open("get", add_link)
  //request.open("get", encode_raw_link(add_link))  // This can be used if the server not percent encoded on it's own.

  request.onreadystatechange = function() {
    if (request.readyState === 4 && request.status === 200) {
      var json = JSON.parse(request.responseText);
      DEBUG && console.log(json)
    } else if (request.status === 0) {
      console.log("Adding file to playlist failed")
    }
  }
  request.send(null)
}

function print_share_list(json){
  // dirname und files-dict
  //DEBUG && console.log(json)

  var sharelist = document.getElementById("sharelist")

  var pEl = sharelist.parentElement

  /* remove from DOM during inserting. Use try-catch to guarantee re-adding.*/
  pEl.removeChild(sharelist)
  try {

    sharelist.replaceChildren()

    var file_dotdot = {list: "..", play: "..", size: -1, modified: -1}


    function add_li(file, idx){
      var isdir = (file.size == -1)
      var li = document.createElement("LI")
      var bullet = document.createElement("I")
      var fname = document.createElement("SPAN")

      var play_link = decode(file.play || "" )
      var list_link = decode(file.list || "")

      function show_playable(el, isplay, isdir) {
        var base_class = isdir?'fa-folder':'fa-file'
        if (isplay){
          el.classList.replace(base_class, 'fa-play')
        }else{
          el.classList.replace('fa-play', base_class)
        }
      }

      li.classList.add('gray')
      bullet.classList.add(idx>=0?'share_bullet':'share_bullet_dotdot')
      if (isdir) {
        bullet.classList.add('fas', 'fa-folder')
      }else{
        bullet.classList.add('fas', 'fa-file')
      }

      //fname.textContent = basename(play_link) // if play_link is not percentage encoded.
      fname.textContent = file.name || decodeURIComponent(basename(play_link))
      if (isdir) {
        fname.classList.add('share_dir')
        fname.addEventListener("click", function() {
          share_change_dir(list_link)
          share_change(document.getElementById("share_selector"))
        })
        if( idx >= 0 ){
          bullet.addEventListener("click", function() {
            share_add_file(play_link, true)
          })
        }
      }else{
        fname.classList.add('share_file')

        if( idx >= 0 ){
          fname.addEventListener("click", function() {
            share_add_file(play_link, false) // playlist_add
          })
          bullet.addEventListener("click", function() {
            share_add_file(play_link, true)  // playlist_play
          })
        }
      }
      if (idx >= 0){
        bullet.addEventListener("mouseenter", function() {show_playable(bullet, 1, isdir)})
        bullet.addEventListener("mouseleave", function() {show_playable(bullet, 0, isdir)})
      }

      li.setAttribute("timestamp", file.modified)
      li.setAttribute("idx", idx)
      //li.setAttribute("isdir", isdir) // Hm, this is always a string
      li.isdir = isdir // this is still Boolean

      li.appendChild(bullet)
      li.appendChild(fname)
      if ( idx >= 0 ){
        var action1 = document.createElement("I")

        action1.classList.add('share_action', 'fas', 'fa-plus-square')
        //action1.textContent = "  [+]"
        action1.addEventListener("click", function() {
          share_add_file(play_link, false)
        })

        li.appendChild(action1)
      }
      if (idx == -2){ // Add button to go up to root dir.
        var bullet2 = document.createElement("I")
        bullet2.classList.add('share_bullet_dotdot', 'fas', 'fa-folder')
        var go_top = document.createElement("SPAN")
        go_top.textContent = "/"
        go_top.classList.add('share_dir')
        go_top.addEventListener("click", function() {
          share_change_dir("/")
          share_change(document.getElementById("share_selector"))
        })
        li.appendChild(bullet2)
        li.appendChild(go_top)
      }

      return li
    }
    // Add .. if not root dir of share
    if (json.dirpath !== shares.list[shares.selected].name_encoded){
      var depth = (json.dirpath.split('/').length > 2)?-2:-1; // First level doesn't need '/' button
      var li_dotdot = add_li(file_dotdot, depth)
      li_dotdot.classList.add('dir_up')
      sharelist.appendChild(li_dotdot)
    }

    var files = json.files
    for(let i = 0; i < files.length; ++i) {
      var file = files[i]
      // Reconstruct full path for list/play
      file.play = `${json.commands.play}/${json.dirpath}/${file.name_encoded}`
      if (file.size == -1){
        file.list = `${json.commands.list}/${json.dirpath}/${file.name_encoded}`
      }

      sharelist.appendChild(add_li(files[i], i))
    }

    /* Presort element before inserting into DOM */
    var local_sorting = shares.local_sorting[json.dirpath]
    if (local_sorting !== undefined){
      DEBUG_SORTINGS && console.log(`Restore sorting ${local_sorting.sname} ${local_sorting.active}`)
      // Use previous sorting option for this folder because the
      // saved scrollingOffset matches just for this value
      shares.sorting = {"sname": local_sorting.sname}
      sortings[local_sorting.sname].active = local_sorting.active
      __sortShareList(sharelist)
      update_sort_buttons()
    }else if(shares.sorting){
      var sname = shares.sorting.sname
      DEBUG_SORTINGS && console.log(`Use sorting ${sname} ${sortings[sname].active}`)
      __sortShareList(sharelist)
    }

  }
  catch(err){
    console.log(err)
  }

  /* Reattach to DOM */
  pEl.appendChild(sharelist)

  /* Reset scrollPosition on saved value, if available.
   * Assumes scarelist.style.overflow == "scroll"
   */
  var sT = shares.scroll_positions[json.dirpath]
  DEBUG && console.log("Set scroll position of share " +
    json.dirpath + " to " + (sT||0))
  sharelist.scrollTop = sT || 0  // sT could be undefined

  // Used name for next storage of scrollTop
  shares.scroll_positions["active_dirpath"] = json.dirpath

}

function __sortShareList(ul) {
  if (shares.sorting === null) return

  var sname = shares.sorting.sname
  var s = sortings[sname]
  if (sname === "alpha" && s.dirs[s.active] == 1){
    // A-Z
    Array.from(ul.getElementsByTagName("LI"))
      .sort((a, b) =>
        {
          if (Number(a.attributes.idx.value) < 0) return -1
          if (Number(b.attributes.idx.value) < 0) return 1
          return a.textContent.localeCompare(b.textContent)
        })
        .forEach(li =>
          ul.appendChild(li))
  }

  if (sname === "alpha" && s.dirs[s.active] == -1){
    // Z-A
    Array.from(ul.getElementsByTagName("LI"))
      .sort((a, b) =>
        {
          if (Number(a.attributes.idx.value) < 0) return -1
          if (Number(b.attributes.idx.value) < 0) return 1
          return -(a.textContent.localeCompare(b.textContent))
        })
        .forEach(li =>
          ul.appendChild(li))
  }

  if (sname === "alpha" && s.dirs[s.active] == 2){
    // A-Z
    Array.from(ul.getElementsByTagName("LI"))
      .sort((a, b) =>
        {
          //var isdir_diff_int = Boolean(b.attributes.isdir) - Boolean(a.attributes.isdir)
          //console.log(`d: ${isdir_diff_int} ${b.attributes.isdir.value} - ${a.attributes.isdir.value}`)
          var isdir_diff_int = b.isdir - a.isdir
          if (isdir_diff_int) return isdir_diff_int;
          if (Number(a.attributes.idx.value) < 0) return -1
          if (Number(b.attributes.idx.value) < 0) return 1
          return a.textContent.localeCompare(b.textContent)
        })
        .forEach(li =>
          ul.appendChild(li))
  }

  if (sname === "alpha" && s.dirs[s.active] == -2){
    // Z-A
    Array.from(ul.getElementsByTagName("LI"))
      .sort((a, b) =>
        {
          var isdir_diff_int = b.isdir - a.isdir
          if (isdir_diff_int) return isdir_diff_int;
          if (Number(a.attributes.idx.value) < 0) return -1
          if (Number(b.attributes.idx.value) < 0) return 1
          return -(a.textContent.localeCompare(b.textContent))
        })
        .forEach(li =>
          ul.appendChild(li))
  }

  if (sname === "date" && s.dirs[s.active] == 1){
    // newest top
    Array.from(ul.getElementsByTagName("LI"))
      .sort((a, b) =>
        {
          if (Number(a.attributes.idx.value) < 0) return -1
          if (Number(b.attributes.idx.value) < 0) return 1
          var ta = Number(a.attributes.timestamp.value)
          var tb = Number(b.attributes.timestamp.value)
          return (tb - ta)
        })
        .forEach(li =>
          ul.appendChild(li))
  }

  if (sname === "date" && s.dirs[s.active] == -1){
    // oldest top
    Array.from(ul.getElementsByTagName("LI"))
      .sort((a, b) =>
        {
          if (Number(a.attributes.idx.value) < 0) return -1
          if (Number(b.attributes.idx.value) < 0) return 1
          var ta = Number(a.attributes.timestamp.value)
          var tb = Number(b.attributes.timestamp.value)
          return -(tb - ta)
        })
        .forEach(li =>
          ul.appendChild(li))
  }
}

function sortShareList() {
  var ul = document.getElementById("sharelist")

  if (ul === null){
    DEBUG && console.log("Hey, sharelist element not found.")
    return
  }

  var pEl = ul.parentElement

  /* remove from DOM during sorting*/
  pEl.removeChild(ul)

  __sortShareList(ul)

  /* Reattach to DOM after sorting */
  pEl.appendChild(ul)
}

function share_change_sorting(sname){
  var s = sortings[sname]
  if (shares.sorting && shares.sorting.sname === sname){
    // go to next index 
    s.active = (s.active + 1) % s.dirs.length
  }else{
    shares.sorting = {"sname": sname}
  }
  update_sort_buttons()
}

function refresh_share_list(){
  var request = new XMLHttpRequest();
  request.open("get", "/media/api/list")

  function _refresh_share_list(json){
    shares.list = []

    var share_selector = document.getElementById("share_selector")
    /*while (share_selector.childElementCount > 0){
    share_selector.removeChild(share_selector.children[0])
  }*/
    share_selector.replaceChildren()

    for (let s in json.shares){
      shares.list.push({"name": s,
        "name_encoded": json.shares[s],
        "url": `${json.commands.list}/${json.shares[s]}`,
        //"dir": "" 
        // '.current': Server return last requested dir for this share
        "dir": ".current"
      })
      var opt = document.createElement('option')
      opt.value = json.shares[s]
      opt.text = s
      share_selector.appendChild(opt)
    }

    share_selector.options.selectedIndex = -1; // Starting unselect
    // to ask server for .current
    share_change(share_selector)
  }


  request.onreadystatechange = function() {
    if (request.readyState === 4 && request.status === 200) {
      var json = JSON.parse(request.responseText)
      _refresh_share_list(json)
    } else if (request.status === 0) {
      console.log("Fetching share list failed")
    }
  }
  request.send(null)
}
