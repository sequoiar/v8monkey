// ----------------------------------------------------------------------------
// Tests installing an signed add-on by navigating directly to the url
function test() {
  Harness.installConfirmCallback = confirm_install;
  Harness.installEndedCallback = install_ended;
  Harness.installsCompletedCallback = finish_test;
  Harness.setup();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.loadURI(TESTROOT + "multipackage.xpi");
}

function get_item(items, name) {
  for (let i = 0; i < items.length; i++) {
    if (items[i].name == name)
      return items[i];
  }
  ok(false, "Item for " + name + " was not listed");
  return null;
}

function confirm_install(window) {
  let items = window.document.getElementById("itemList").childNodes;
  is(items.length, 2, "Should be 2 items listed in the confirmation dialog");

  let item = get_item(items, "XPI Test");
  if (item) {
    is(item.signed, "false", "Should not have listed the item as signed");
    is(item.icon, "", "Should have listed no icon for the item");
  }

  item = get_item(items, "Signed XPI Test");
  if (item) {
    is(item.cert, "(Object Signer)", "Should have seen the signer");
    is(item.signed, "true", "Should have listed the item as signed");
    is(item.icon, "", "Should have listed no icon for the item");
  }

  return true;
}

function install_ended(install, addon) {
  install.cancel();
}

function finish_test(count) {
  is(count, 2, "2 Add-ons should have been successfully installed");
  gBrowser.removeCurrentTab();
  Harness.finish();
}
// ----------------------------------------------------------------------------
