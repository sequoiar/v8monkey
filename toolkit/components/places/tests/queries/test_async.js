/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Drew Willcoxon <adw@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let gTests = [
  {
    desc: "nsNavHistoryFolderResultNode: Basic test, asynchronously open and " +
          "close container with a single child",

    loading: function (node, newState, oldState) {
      this.checkStateChanged("loading", 1);
      this.checkArgs("loading", node, oldState, node.STATE_CLOSED);
    },

    opened: function (node, newState, oldState) {
      this.checkStateChanged("opened", 1);
      this.checkState("loading", 1);
      this.checkArgs("opened", node, oldState, node.STATE_LOADING);

      print("Checking node children");
      compareArrayToResult(this.data, node);

      print("Closing container");
      node.containerOpen = false;
    },

    closed: function (node, newState, oldState) {
      this.checkStateChanged("closed", 1);
      this.checkState("opened", 1);
      this.checkArgs("closed", node, oldState, node.STATE_OPENED);
      this.success();
    }
  },

  {
    desc: "nsNavHistoryFolderResultNode: After async open and no changes, " +
          "second open should be synchronous",

    loading: function (node, newState, oldState) {
      this.checkStateChanged("loading", 1);
      this.checkState("closed", 0);
      this.checkArgs("loading", node, oldState, node.STATE_CLOSED);
    },

    opened: function (node, newState, oldState) {
      let cnt = this.checkStateChanged("opened", 1, 2);
      let expectOldState = cnt === 1 ? node.STATE_LOADING : node.STATE_CLOSED;
      this.checkArgs("opened", node, oldState, expectOldState);

      print("Checking node children");
      compareArrayToResult(this.data, node);

      print("Closing container");
      node.containerOpen = false;
    },

    closed: function (node, newState, oldState) {
      let cnt = this.checkStateChanged("closed", 1, 2);
      this.checkArgs("closed", node, oldState, node.STATE_OPENED);

      switch (cnt) {
      case 1:
        node.containerOpen = true;
        break;
      case 2:
        this.success();
        break;
      }
    }
  },

  {
    desc: "nsNavHistoryFolderResultNode: After closing container in " +
          "loading(), opened() should not be called",

    loading: function (node, newState, oldState) {
      this.checkStateChanged("loading", 1);
      this.checkArgs("loading", node, oldState, node.STATE_CLOSED);
      print("Closing container");
      node.containerOpen = false;
    },

    opened: function (node, newState, oldState) {
      do_throw("opened should not be called");
    },

    closed: function (node, newState, oldState) {
      this.checkStateChanged("closed", 1);
      this.checkState("loading", 1);
      this.checkArgs("closed", node, oldState, node.STATE_LOADING);
      this.success();
    }
  }
];


/**
 * Instances of this class become the prototypes of the test objects above.
 * Each test can therefore use the methods of this class, or they can override
 * them if they want.  To run a test, call setup() and then run().
 */
function Test() {
  // This maps a state name to the number of times it's been observed.
  this.stateCounts = {};
}

Test.prototype = {
  /**
   * Call this when an observer observes a container state change to sanity
   * check the arguments.
   *
   * @param aNewState
   *        The name of the new state.  Used only for printing out helpful info.
   * @param aNode
   *        The node argument passed to containerStateChanged.
   * @param aOldState
   *        The old state argument passed to containerStateChanged.
   * @param aExpectOldState
   *        The expected old state.
   */
  checkArgs: function (aNewState, aNode, aOldState, aExpectOldState) {
    print("Node passed on " + aNewState + " should be result.root");
    do_check_eq(this.result.root, aNode);
    print("Old state passed on " + aNewState + " should be " + aExpectOldState);

    // aOldState comes from xpconnect and will therefore be defined.  It may be
    // zero, though, so use strict equality just to make sure aExpectOldState is
    // also defined.
    do_check_true(aOldState === aExpectOldState);
  },

  /**
   * Call this when an observer observes a container state change.  It registers
   * the state change and ensures that it has been observed the given number
   * of times.  See checkState for parameter explanations.
   *
   * @return The number of times aState has been observed, including the new
   *         observation.
   */
  checkStateChanged: function (aState, aExpectedMin, aExpectedMax) {
    print(aState + " state change observed");
    if (!this.stateCounts.hasOwnProperty(aState))
      this.stateCounts[aState] = 0;
    this.stateCounts[aState]++;
    return this.checkState(aState, aExpectedMin, aExpectedMax);
  },

  /**
   * Ensures that the state has been observed the given number of times.
   *
   * @param  aState
   *         The name of the state.
   * @param  aExpectedMin
   *         The state must have been observed at least this number of times.
   * @param  aExpectedMax
   *         The state must have been observed at most this number of times.
   *         This parameter is optional.  If undefined, it's set to
   *         aExpectedMin.
   * @return The number of times aState has been observed, including the new
   *         observation.
   */
  checkState: function (aState, aExpectedMin, aExpectedMax) {
    let cnt = this.stateCounts[aState] || 0;
    if (aExpectedMax === undefined)
      aExpectedMax = aExpectedMin;
    if (aExpectedMin === aExpectedMax) {
      print(aState + " should be observed only " + aExpectedMin +
            " times (actual = " + cnt + ")");
    }
    else {
      print(aState + " should be observed at least " + aExpectedMin +
            " times and at most " + aExpectedMax + " times (actual = " +
            cnt + ")");
    }
    do_check_true(cnt >= aExpectedMin && cnt <= aExpectedMax);
    return cnt;
  },

  /**
   * Asynchronously opens the root of the test's result.
   */
  openContainer: function () {
    // Set up the result observer.  It delegates to this object's callbacks and
    // wraps them in a try-catch so that errors don't get eaten.
    this.observer = let (self = this) {
      containerStateChanged: function (container, oldState, newState) {
        print("New state passed to containerStateChanged() should equal the " +
              "container's current state");
        do_check_eq(newState, container.state);

        try {
          switch (newState) {
          case Ci.nsINavHistoryContainerResultNode.STATE_LOADING:
            self.loading(container, newState, oldState);
            break;
          case Ci.nsINavHistoryContainerResultNode.STATE_OPENED:
            self.opened(container, newState, oldState);
            break;
          case Ci.nsINavHistoryContainerResultNode.STATE_CLOSED:
            self.closed(container, newState, oldState);
            break;
          default:
            do_throw("Unexpected new state! " + newState);
          }
        }
        catch (err) {
          do_throw(err);
        }
      },
      containerOpened: function (container) {},
      containerClosed: function (container) {}
    };
    this.result.addObserver(this.observer, false);

    print("Opening container");
    this.result.root.containerOpen = true;
  },

  /**
   * Override this if need be.
   */
  run: function () {
    this.openContainer();
  },

  /**
   * This must be called before run().  It adds a bookmark and sets up the
   * test's result.  Override if need be.
   */
  setup: function () {
    // Populate the database with different types of bookmark items.
    this.data = DataHelper.makeDataArray([
      { type: "bookmark" },
      { type: "separator" },
      { type: "folder" },
      { type: "bookmark", uri: "place:terms=foo" }
    ]);
    populateDB(this.data);

    // Make a query.
    this.query = PlacesUtils.history.getNewQuery();
    this.query.setFolders([DataHelper.defaults.bookmark.parent], 1);
    this.opts = PlacesUtils.history.getNewQueryOptions();
    this.opts.asyncEnabled = true;
    this.result = PlacesUtils.history.executeQuery(this.query, this.opts);
  },

  /**
   * Call this when the test has succeeded.  It cleans up resources and starts
   * the next test.
   */
  success: function () {
    this.result.removeObserver(this.observer);
    doNextTest();
  }
};

/**
 * This makes it a little bit easier to use the functions of head_queries.js.
 */
let DataHelper = {
  defaults: {
    bookmark: {
      parent: PlacesUtils.bookmarks.unfiledBookmarksFolder,
      uri: "http://example.com/",
      title: "test bookmark"
    },

    folder: {
      parent: PlacesUtils.bookmarks.unfiledBookmarksFolder,
      title: "test folder"
    },

    separator: {
      parent: PlacesUtils.bookmarks.unfiledBookmarksFolder
    }
  },

  /**
   * Converts an array of simple bookmark item descriptions to the more verbose
   * format required by populateDB() in head_queries.js.
   *
   * @param  aData
   *         An array of objects, each of which describes a bookmark item.
   * @return An array of objects suitable for passing to populateDB().
   */
  makeDataArray: function DH_makeDataArray(aData) {
    return let (self = this) aData.map(function (dat) {
      let type = dat.type;
      dat = self._makeDataWithDefaults(dat, self.defaults[type]);
      switch (type) {
      case "bookmark":
        return {
          isBookmark: true,
          uri: dat.uri,
          parentFolder: dat.parent,
          index: PlacesUtils.bookmarks.DEFAULT_INDEX,
          title: dat.title,
          isInQuery: true
        };
      case "separator":
        return {
          isSeparator: true,
          parentFolder: dat.parent,
          index: PlacesUtils.bookmarks.DEFAULT_INDEX,
          isInQuery: true
        };
      case "folder":
        return {
          isFolder: true,
          readOnly: false,
          parentFolder: dat.parent,
          index: PlacesUtils.bookmarks.DEFAULT_INDEX,
          title: dat.title,
          isInQuery: true
        };
      default:
        do_throw("Unknown data type when populating DB: " + type);
      }
    });
  },

  /**
   * Returns a copy of aData, except that any properties that are undefined but
   * defined in aDefaults are set to the corresponding values in aDefaults.
   *
   * @param  aData
   *         An object describing a bookmark item.
   * @param  aDefaults
   *         An object describing the default bookmark item.
   * @return A copy of aData with defaults values set.
   */
  _makeDataWithDefaults: function DH__makeDataWithDefaults(aData, aDefaults) {
    let dat = {};
    for (let [prop, val] in Iterator(aDefaults)) {
      dat[prop] = aData.hasOwnProperty(prop) ? aData[prop] : val;
    }
    return dat;
  }
};

function doNextTest() {
  remove_all_bookmarks();
  if (gTests.length === 0) {
    print("All tests done, exiting");
    do_test_finished();
  }
  else {
    let test = gTests.shift();
    test.__proto__ = new Test();
    test.setup();
    print("------ Running test: " + test.desc);
    test.run();
  }
}

function run_test() {
  do_test_pending();
  doNextTest();
}
