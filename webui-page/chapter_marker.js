let delayed_chapter_marks = false // to show chapters after status update provides all ness. info.

function updateCapterMarks(num_chapters, chapter, metadata, chapter_list, duration, time_pos) {
  /* Enrich the position slider by the chapter marks. Currently
   * this is realized by drawing the marks into a SVG background-image.
   */
  var chapter_marker = document.getElementById('chapter_marker')
  if (num_chapters === 0) {
    chapter_marker.style.backgroundImage = 'none'
  }else{
    if( chapter == -1 && time_pos < 0.1){
      /* If mpv is paused and the played file is changed, duration of the previous file is
       * still the active. We need to wait until chapter >= 0 or time-pos > 0
       * to guarantee that the correct
       * time was extracted from the file. (*4)
       *
       */
      delayed_chapter_marks = true
      return;
    }

    var svg_chapter_list = []
    for (let i = 0; i < chapter_list.length; i++){
      const pos_promille = (1000 * chapter_list[i]['time']/duration).toFixed(2)
      svg_chapter_list.push(`<use xlink:href="%23markB" x="${pos_promille}" width="50" height="100%" />\\`)
    }
    chapter_marker.style.backgroundImage = `url('data:image/svg+xml,\\
<svg version="1.1" \\
xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" \\
width="1100" height="auto" preserveAspectRatio="none">\\
<defs>\\
<symbol id="markB" \\
viewBox="0 0 50 200" \\
preserveAspectRatio="xMidYMid" \\
>\\
<polygon \\
points="24,49 0,0 49,0, 25,49, 25,150, 49,199, 0,199, 24,150" \\
fill="rgba(200,30,30, 0.3)" \\
stroke="%23555544" stroke-width="1" vector-effect="non-scaling-stroke"\\
/>\\
</symbol>\\
</defs>\\
<g transform="translate(25 0)">\\
    ${svg_chapter_list.join('\n')}
</g>\\
</svg>')`
  }
}

