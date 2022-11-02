var sortings = {
  "alpha": {
    // id for direction, normally just 1 for forward and -1 for backward
    "dirs": [-1, 1],
    "icons": ["fa-sort-alpha-up", "fa-sort-alpha-down"],
    "active": 0 //index of above lists
  },
  "date": {
    "dirs": [-1, 1],
    "icons": ["fa-sort-numeric-up", "fa-sort-numeric-down"],
    "active": 0 //index of above lists
  }
}

var shares = {
  list: null,
  sorting: {'sname':'alpha'},
  selected: null,
  close_event_registered: false,
  scroll_positions: {}
}

function toggleShares() {
  document.body.classList.toggle('noscroll')
  var el = document.getElementById("overlay2")
  el.style.visibility = (el.style.visibility === "visible") ? "hidden" : "visible"
  if (el.style.visibility === "visible" ){
    if (shares.list == null){
      refresh_share_list()
    }
  }

  /* Close share overlay by click on background area. */
  if (!shares.close_event_registered ){
    function _close_listener(evt) {
      console.log(evt.target)
      if( evt.target == el ){
        toggleShares()
      }
      evt.stopPropagation();
    }
    el.addEventListener('click', _close_listener, false)
    shares.close_event_registered = true
  }

}

function share_change(el){
  /* Save current scroll position */
  var sharelist = document.getElementById("sharelist")
  var prev_dirname = shares.scroll_positions["active_dirname"]
  if( prev_dirname !== undefined ){
    var sT = sharelist.scrollTop;
    DEBUG && console.log("Save scroll position of share " +
      prev_dirname + " to " + sT)
    shares.scroll_positions[prev_dirname] = sT
  }

  var value = el.options[el.selectedIndex].value
  var text = el.options[el.selectedIndex].text
  var s = shares.list[el.selectedIndex]
  var request = new XMLHttpRequest();

  shares.selected = el.selectedIndex
  request.open("get", s["url"] + "/" + s["dir"] )

  request.onreadystatechange = function() {
    if (request.readyState === 4 && request.status === 200) {
      var json = JSON.parse(request.responseText)
      print_share_list(json)
    } else if (request.status === 0) {
      console.log("Fetching share list failed")
    }
  }
  request.send(null)
}

function share_change_dir(list_link){
  if (list_link === "..") {
    var cur_dir = shares.list[shares.selected].url + "/" 
      + shares.list[shares.selected].dir
      .replace(/[/]+$/gm,""); // remove '/' at end

    var last_token = basename(cur_dir)
    var parent_dir = cur_dir.substring(0, cur_dir.length - last_token.length)
    list_link = parent_dir
  }
  DEBUG && console.log("Link of new share dir: " + list_link)

  var new_dir = share_get_subdir(list_link);
  DEBUG && console.log("Relative dir of new share dir: " + new_dir)

  shares.list[shares.selected].dir = new_dir
}

function share_get_subdir(dirname){
  /* strips prefix /media/api/list/<sharename> from dirname 
   * and remove leading and trailing '/'.
   */

  var root = shares.list[shares.selected].url
  if (!dirname.startsWith(root)) return false

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
    add_link = add_link.replace("playlist_add","playlist_play")
  }

  var request = new XMLHttpRequest();
  request.open("get", add_link )

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
  sharelist.replaceChildren()

  var file_dotdot = {list: "..", play: "..", size: -1, modified: -1}


  function add_li(file, idx){
    var isdir = (file.size == -1)
    var li = document.createElement("LI")
    var bullet = document.createElement("I")
    var fname = document.createElement("SPAN")
    var action1 = document.createElement("I")

    var play_link = decode(file.play)

    li.classList.add('gray')
    bullet.classList.add('share_bullet')
    if (isdir) {
      bullet.classList.add('fas')
      bullet.classList.add('fa-folder')
    }else{
      bullet.classList.add('fas')
      bullet.classList.add('fa-file')
    }

    fname.textContent = basename(play_link)
    if (isdir) {
      fname.classList.add('share_dir')
      fname.addEventListener("click", function() {
        share_change_dir(file.list)
        share_change(document.getElementById("share_selector"))
      })
    }else{
      fname.classList.add('share_file')

      if( idx >= 0 ){
        fname.addEventListener("click", function() {
          share_add_file(play_link, true)
        })
      }
    }

    action1.classList.add('share_action')
    action1.classList.add('fas')
    action1.classList.add('fa-plus-square')
    //action1.textContent = "  [+]"
    action1.addEventListener("click", function() {
      share_add_file(play_link, false)
    })

    li.setAttribute("timestamp", file.modified)
    li.setAttribute("idx", idx)

    li.appendChild(bullet)
    li.appendChild(fname)
    if ( idx >= 0 ){
      li.appendChild(action1)
    }

    return li
  }

  if (shares.list[shares.selected].dir === ".current"){
    DEBUG && console.log("Replace .current by "
      + share_get_subdir(json.dirname))
    shares.list[shares.selected].dir = share_get_subdir(json.dirname)
  }

  // Add .. if not root dir of share
  if (json.dirname !== shares.list[shares.selected].url){
    var li_dotdot = add_li(file_dotdot, -1)
    li_dotdot.classList.add('dir_up')
    sharelist.appendChild(li_dotdot)
  }

  var files = json.files
  for(var i = 0; i < files.length; ++i) {
    sharelist.appendChild(add_li(files[i], i))
  }

  /* Reset scrollPosition on saved value, if available.
   * Assumes scarelist.style.overflow == "scroll"
   */ 
  var sT = shares.scroll_positions[json.dirname]
  DEBUG && console.log("Set scroll position of share " + 
    json.dirname + " to " + sT)
  if (sT !== undefined){
    sharelist.scrollTop = sT
  }else {
    sharelist.scrollTop = 0
  }
  // Used name for next storage of scrollTop
  shares.scroll_positions["active_dirname"] = json.dirname

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

  /* Reattach to DOM after sorting */
  pEl.appendChild(ul)
}

function share_change_sorting(sname){
  var s = sortings[sname]
  if (shares.sorting.sname === sname){
    // go to next index 
    s.active = (s.active + 1) % s.dirs.length
  }else{
    shares.sorting.sname = sname
  }
  update_sort_buttons()
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

  for (var sname in sortings ){
    var s = sortings[sname]
    var cur = s.active
    var prev = (s.dirs.length + cur - 1) % s.dirs.length
    var x = get_fa_nodes("shareSortButton_" + sname, "fas")
    x.forEach(function (fasEl) {
      fasEl.classList.replace(s["icons"][prev], s["icons"][cur])
    })
  }
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

    for (var s in json.shares){
      shares.list.push({"name": s,
        "url": json.shares[s],
        //"dir": "" 
        // '.current': Server return last requested dir for this share
        "dir": ".current"
      })
      var opt = document.createElement('option')
      opt.value = json.shares[s]
      opt.text = s
      share_selector.appendChild(opt)
    }

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

function decode(s){
  /* json was generated by otemplate and contains
   * encoded html chars.
   *
   * Without changing otemplate tool we just
   * can undo this by reverting onion_html_add_enc().
   */

  /*var doc = new DOMParser().parseFromString(s, "text/html");
    return doc.documentElement.textContent;
    */
  // or
  s = s.replace('&amp;', '&')
  s = s.replace('&#39;', '\'').replace('&quot;','"')
  s = s.replace('&lt;', '<').replace('&gt;', '>')
  return s;
}
