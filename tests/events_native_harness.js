// Minimal assertions for replaying the same public contract under JerryScript.
// Node runs events.test.js with node:assert; this runner has no Node dependency.
var completedEventTests = 0;
function test(name, body) {
  try { body(); completedEventTests++; }
  catch (error) { throw new Error(name + ': ' + error); }
}
var assert = {
  equal: function (actual, expected) {
    if (actual !== expected) throw new Error('Expected ' + expected + ', got ' + actual);
  },
  deepEqual: function (actual, expected) {
    // The shared tests compare only JSON-shaped arrays and literal limit maps.
    if (JSON.stringify(actual) !== JSON.stringify(expected)) throw new Error('JSON values differ');
  },
  throws: function (body, expected) {
    var caught = false;
    try { body(); }
    catch (error) {
      caught = true;
      if (expected.prototype instanceof Error) {
        if (!(error instanceof expected)) throw new Error('Wrong error type: ' + error);
      } else if (!expected(error)) throw new Error('Error predicate failed');
    }
    if (!caught) throw new Error('Expected an exception');
  }
};
