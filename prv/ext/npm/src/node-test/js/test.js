#!/usr/bin/node

var assert = require('assert');
var addon = require('libblank');

console.log(addon.sayHello(7, "8"));

console.log(addon.showNum(99));

addon.noReturn(5);

var ct = new addon.CTestClass();

ct.setX(88);
console.log(ct.getX());
assert.equal(88, ct.getX());

var ct2 = new addon.CTestClass(77);
console.log(ct2.getX());
assert.equal(77, ct2.getX());
