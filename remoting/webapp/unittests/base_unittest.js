// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

module('base');

test('mix(dest, src) should copy properties from |src| to |dest|',
  function() {
    var src = { a: 'a', b: 'b'};
    var dest = { c: 'c'};

    base.mix(dest, src);
    deepEqual(dest, {a: 'a', b: 'b', c: 'c'});
});

test('mix(dest, src) should assert if properties are overwritten',
  function() {
    var src = { a: 'a', b: 'b'};
    var dest = { a: 'a'};

    sinon.spy(base.debug, 'assert');

    try {
      base.mix(dest, src);
    } catch (e) {
    } finally {
      sinon.assert.called(base.debug.assert);
      base.debug.assert.restore();
    }
});

test('values(obj) should return an array containing the values of |obj|',
  function() {
    var output = base.values({ a: 'a', b: 'b'});

    notEqual(output.indexOf('a'), -1, '"a" should be in the output');
    notEqual(output.indexOf('b'), -1, '"b" should be in the output');
});

test('dispose(obj) should invoke the dispose method on |obj|',
  function() {
    var obj = {
      dispose: sinon.spy()
    };
    base.dispose(obj);
    sinon.assert.called(obj.dispose);
});

test('dispose(obj) should not crash if |obj| is null',
  function() {
    expect(0);
    base.dispose(null);
});

QUnit.asyncTest('Promise.sleep(delay) should fulfill the promise after |delay|',
  function() {
    var isCalled = false;
    var clock = this.clock;

    base.Promise.sleep(100).then(function(){
      isCalled = true;
      ok(true, 'Promise.sleep() is fulfilled after delay.');
      QUnit.start();
    });

    // Tick the clock for 2 seconds and check if the promise is fulfilled.
    clock.tick(2);

    // Promise fulfillment always occur on a new stack.  Therefore, we will run
    // the verification in a requestAnimationFrame.
    window.requestAnimationFrame(function(){
      ok(!isCalled, 'Promise.sleep() should not be fulfilled prematurely.');
      clock.tick(101);
    }.bind(this));
});


var source = null;
var listener = null;

module('base.EventSource', {
  setup: function() {
    source = new base.EventSource();
    source.defineEvents(['foo', 'bar']);
    listener = sinon.spy();
    source.addEventListener('foo', listener);
  },
  teardown: function() {
    source = null;
    listener = null;
  }
});

test('raiseEvent() should invoke the listener', function() {
  source.raiseEvent('foo');
  sinon.assert.called(listener);
});

test('raiseEvent() should invoke the listener with the correct event data',
  function() {
    var data = {
      field: 'foo'
    };
    source.raiseEvent('foo', data);
    sinon.assert.calledWith(listener, data);
});

test(
  'raiseEvent() should not invoke listeners that are added during raiseEvent',
  function() {
    source.addEventListener('foo', function() {
      source.addEventListener('foo', function() {
        ok(false);
      });
      ok(true);
    });
    source.raiseEvent('foo');
});

test('raiseEvent() should not invoke listeners of a different event',
  function() {
    source.raiseEvent('bar');
    sinon.assert.notCalled(listener);
});

test('raiseEvent() should assert when undeclared events are raised',
  function() {
    sinon.spy(base.debug, 'assert');
    try {
      source.raiseEvent('undefined');
    } catch (e) {
    } finally {
      sinon.assert.called(base.debug.assert);
      base.debug.assert.restore();
    }
});

test(
  'removeEventListener() should not invoke the listener in subsequent ' +
  'calls to |raiseEvent|',
  function() {
    source.raiseEvent('foo');
    sinon.assert.calledOnce(listener);

    source.removeEventListener('foo', listener);
    source.raiseEvent('foo');
    sinon.assert.calledOnce(listener);
});

test('removeEventListener() should work even if the listener ' +
  'is removed during |raiseEvent|',
  function() {
    var sink = {};
    sink.listener = sinon.spy(function() {
      source.removeEventListener('foo', sink.listener);
    });

    source.addEventListener('foo', sink.listener);
    source.raiseEvent('foo');
    sinon.assert.calledOnce(sink.listener);

    source.raiseEvent('foo');
    sinon.assert.calledOnce(sink.listener);
});

})();
