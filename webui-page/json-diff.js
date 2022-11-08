/* Rudimentary detection of differences between objects
 *
 * Used to track changes in playlists.
 *
 * Returns list of changes need to applied
 *
 * [{ op: "add|remove|replace", new: 8, old: 8}, [...]]
 */

function get_diff(new_list, old_list, compare_handler){
  var nNew = new_list.length
  var nOld = old_list.length
  var operations = []
  var iNew = 0 ,iOld = 0;
  while (iNew<nNew && iOld<nOld) {
    if (compare_handler(new_list[iNew], old_list[iOld])){
      ++iNew; ++iOld; continue;
    }
    // check if next entries are the same. => Replace operation
    if (new_list[iNew+1] && old_list[iOld+1] &&
      compare_handler(new_list[iNew+1], old_list[iOld+1])){
      operations.push({'op': 'replace', 'new': iNew, 'old': iOld})
      iNew+=2
      iOld+=2
      continue
    }

    // Next new is current old entry => Insert operation
    if (new_list[iNew+1] &&
      compare_handler(new_list[iNew+1], old_list[iOld])){
      operations.push({'op': 'add', 'new': iNew, 'old': iOld})
      iNew+=2
      iOld+=1
      continue
    }

    // Next old is current new entry => Delete operation
    if (old_list[iOld+1] &&
      compare_handler(new_list[iNew], old_list[iOld+1])){
      operations.push({'op': 'remove', 'new': -1, 'old': iOld})
      iNew+=1
      iOld+=2
      continue
    }

    // Assume replacement and continue with next elements
    operations.push({'op': 'replace', 'new': iNew, 'old': iOld})
    iNew+=1
    iOld+=1

  }

  // Remove more pending entries…
  while (iOld<nOld) {
    operations.push({'op': 'remove', 'new': -1, 'old': iOld})
    ++iOld;
  }

  // …or add more new entries
  while (iNew<nNew) {
    operations.push({'op': 'add', 'new': iNew, 'old': nOld})
    ++iNew;
    ++nOld
  }

  return operations
}

