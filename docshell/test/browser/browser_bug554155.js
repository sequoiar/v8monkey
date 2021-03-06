function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab("http://example.com");

  tab.linkedBrowser.addEventListener('load', function() {
    let numLocationChanges = 0;

    let listener = {
      onLocationChange: function() {
        numLocationChanges++;
      }
    };

    gBrowser.addTabsProgressListener(listener);

    // pushState to a new URL (http://example.com/foo").  This should trigger
    // exactly one LocationChange event.
    tab.linkedBrowser.contentWindow.history.pushState(null, null, "foo");

    executeSoon(function() {
      gBrowser.removeTab(tab);
      gBrowser.removeTabsProgressListener(listener);
      is(numLocationChanges, 1,
         "pushState should cause exactly one LocationChange event.");
      finish();
    });

  }, true);
}
