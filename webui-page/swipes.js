
/* Adds swipe handlers for all overlays (if oname === undefined)
 * or given overlay id */
function add_overlay_swipes(oname){

  /*document.addEventListener('swiped', function(e) {
    console.log(e.target); // element that was swiped
    console.log(e.detail); // see event data below
    console.log(e.detail.dir); // swipe direction
  })
  console.log("SWIPE registered")
  return*/

  let onames = Object.keys(overlays)
  let n = onames.length
  if (n < 2) return
  onames.push(onames[0])   // First at end 
  onames.unshift(onames[n-1])// Last at begin

  function _overlay_switch(hmap){
    return function(evt){
      let h = hmap[evt.detail.dir]
      if (h !== undefined) h(evt)
    }
  }
  for(let i = 1; i < n+1; ++i) {
    if (oname && oname !== onames[i]) continue
    DEBUG && console.log("Add swipe event listener")

    function _switch_overlay(from, to){
      let hFrom = overlays[onames[from]]
      let hTo = overlays[onames[to]]
      return function (){
        hFrom(false)
        hTo(true)
      }
    }

    let hmap = {}

    if (overlays[onames[i]] === toggleShares){
      // refresh share with swipeDown and switch Share with two fingers + left/right
      hmap.left = function(evt) {
        if (evt.detail.fingers > 1){
          cycle_share(-1)
        }else{
          _switch_overlay(i, i-1)()
        }
      }
      hmap.right = function(evt) {
        if (evt.detail.fingers > 1){
          cycle_share(1)
        }else{
          _switch_overlay(i, i+1)()
        }
      }
      //if (onames[i] === "overlay2"){
      hmap["down"] = function(){
        DEBUG && console.log("Refresh current share directory");
        share_change_dir('.')
        share_change(document.getElementById("share_selector"))
      }
      /*hmap["up"] = function(){
        DEBUG && console.log("Up");
        cycle_share(1)
      }
      */
    }else{
      // Just left/right swipe to next overlay
      hmap.left = _switch_overlay(i, i-1)
      hmap.right = _switch_overlay(i, i+1)
    }

    document.getElementById(onames[i]).addEventListener('swiped',
      _overlay_switch(hmap))

    /*document.addEventListener('swiped', function(e) {
      console.log(e.target); // element that was swiped
      console.log(e.detail); // see event data below
      console.log(e.detail.dir); // swipe direction
    })
    document.getElementById(onames[i]).addEventListener('swiped', function(e) {
      console.log(e.target); // element that was swiped
      console.log(e.detail); // see event data below
      console.log(e.detail.dir); // swipe direction
    })*/
    document.getElementById(onames[i]).addEventListener('scroll', function() {
      console.log("Scroll");
    }, false);
  }
}

