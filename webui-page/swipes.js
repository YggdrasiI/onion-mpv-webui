
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
      if (h !== undefined) h()
    }
  }
  for(let i = 1; i < n+1; ++i) {
    if (oname && oname !== onames[i]) continue
    DEBUG && console.log("Add swipe event listener")

    let hmap = {
      //left: function(){ toggleOverlay(onames[i], false); toggleOverlay(onames[i-1], true)},
      left: function(){ overlays[onames[i]](false); overlays[onames[i-1]](true)},
      right: function(){ overlays[onames[i]](false); overlays[onames[i+1]](true)},
    }

    if (overlays[onames[i]] === toggleShares){
      //if (onames[i] === "overlay2"){
      hmap["down"] = function(){
        DEBUG && console.log("Refresh current share directory");
        share_change_dir('.')
      }
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

