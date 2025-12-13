/* Polyfiller for Duktape for NetSurf
 *
 * This JavaScript will be loaded into heaps before the generics
 *
 * We only care for the side-effects of this, be careful.
 */

// Production steps of ECMA-262, Edition 6, 22.1.2.1
if (!Array.from) {
  Array.from = (function () {
    var toStr = Object.prototype.toString;
    var isCallable = function (fn) {
      return typeof fn === 'function' || toStr.call(fn) === '[object Function]';
    };
    var toInteger = function (value) {
      var number = Number(value);
      if (isNaN(number)) { return 0; }
      if (number === 0 || !isFinite(number)) { return number; }
      return (number > 0 ? 1 : -1) * Math.floor(Math.abs(number));
    };
    var maxSafeInteger = Math.pow(2, 53) - 1;
    var toLength = function (value) {
      var len = toInteger(value);
      return Math.min(Math.max(len, 0), maxSafeInteger);
    };

    // The length property of the from method is 1.
    return function from(arrayLike/*, mapFn, thisArg */) {
      // 1. Let C be the this value.
      var C = this;

      // 2. Let items be ToObject(arrayLike).
      var items = Object(arrayLike);

      // 3. ReturnIfAbrupt(items).
      if (arrayLike == null) {
        throw new TypeError('Array.from requires an array-like object - not null or undefined');
      }

      // 4. If mapfn is undefined, then let mapping be false.
      var mapFn = arguments.length > 1 ? arguments[1] : void undefined;
      var T;
      if (typeof mapFn !== 'undefined') {
        // 5. else
        // 5. a If IsCallable(mapfn) is false, throw a TypeError exception.
        if (!isCallable(mapFn)) {
          throw new TypeError('Array.from: when provided, the second argument must be a function');
        }

        // 5. b. If thisArg was supplied, let T be thisArg; else let T be undefined.
        if (arguments.length > 2) {
          T = arguments[2];
        }
      }

      // 10. Let lenValue be Get(items, "length").
      // 11. Let len be ToLength(lenValue).
      var len = toLength(items.length);

      // 13. If IsConstructor(C) is true, then
      // 13. a. Let A be the result of calling the [[Construct]] internal method 
      // of C with an argument list containing the single item len.
      // 14. a. Else, Let A be ArrayCreate(len).
      var A = isCallable(C) ? Object(new C(len)) : new Array(len);

      // 16. Let k be 0.
      var k = 0;
      // 17. Repeat, while k < len… (also steps a - h)
      var kValue;
      while (k < len) {
        kValue = items[k];
        if (mapFn) {
          A[k] = typeof T === 'undefined' ? mapFn(kValue, k) : mapFn.call(T, kValue, k);
        } else {
          A[k] = kValue;
        }
        k += 1;
      }
      // 18. Let putStatus be Put(A, "length", len, true).
      A.length = len;
      // 20. Return A.
      return A;
    };
  }());
}

// DOMTokenList formatter, in theory we can remove this if we do the stringifier IDL support

DOMTokenList.prototype.toString = function () {
  if (this.length == 0) {
    return "";
  }

  var ret = this.item(0);
  for (var index = 1; index < this.length; index++) {
    ret = ret + " " + this.item(index);
  }

  return ret;
}

// Inherit the same toString for settable lists
DOMSettableTokenList.prototype.toString = DOMTokenList.prototype.toString;

// Console stub
if (typeof console === 'undefined') {
    this.console = {
        log: function() {},
        debug: function() {},
        info: function() {},
        warn: function() {},
        error: function() {},
        assert: function() {},
        dir: function() {},
        dirxml: function() {},
        trace: function() {},
        group: function() {},
        groupCollapsed: function() {},
        groupEnd: function() {},
        time: function() {},
        timeEnd: function() {},
        profile: function() {},
        profileEnd: function() {},
        count: function() {}
    };
}

// Performance stub
if (typeof performance === 'undefined') {
    this.performance = {
        now: function() {
            return Date.now();
        }
    };
}

// requestAnimationFrame stub
if (typeof requestAnimationFrame === 'undefined') {
    this.requestAnimationFrame = function(callback) {
        return setTimeout(function() {
            callback(performance.now());
        }, 16);
    };
    this.cancelAnimationFrame = function(id) {
        clearTimeout(id);
    };
}

// Minimal Promise polyfill
if (typeof Promise === 'undefined') {
    (function(global) {
        function Promise(executor) {
            var self = this;
            self.state = 'pending';
            self.value = undefined;
            self.callbacks = [];

            function resolve(value) {
                if (self.state !== 'pending') return;
                self.state = 'fulfilled';
                self.value = value;
                self.callbacks.forEach(function(callback) {
                    callback.onFulfilled(value);
                });
            }

            function reject(reason) {
                if (self.state !== 'pending') return;
                self.state = 'rejected';
                self.value = reason;
                self.callbacks.forEach(function(callback) {
                    callback.onRejected(reason);
                });
            }

            try {
                executor(resolve, reject);
            } catch (e) {
                reject(e);
            }
        }

        Promise.prototype.then = function(onFulfilled, onRejected) {
            var self = this;
            return new Promise(function(resolve, reject) {
                function handle(callback, value) {
                    try {
                        var result = callback ? callback(value) : value;
                        if (result instanceof Promise) {
                            result.then(resolve, reject);
                        } else {
                            resolve(result);
                        }
                    } catch (e) {
                        reject(e);
                    }
                }

                if (self.state === 'pending') {
                    self.callbacks.push({
                        onFulfilled: function(value) {
                            if (onFulfilled) handle(onFulfilled, value);
                            else resolve(value);
                        },
                        onRejected: function(reason) {
                            if (onRejected) handle(onRejected, reason);
                            else reject(reason);
                        }
                    });
                } else if (self.state === 'fulfilled') {
                    if (onFulfilled) setTimeout(function() { handle(onFulfilled, self.value); }, 0);
                    else resolve(self.value);
                } else if (self.state === 'rejected') {
                    if (onRejected) setTimeout(function() { handle(onRejected, self.value); }, 0);
                    else reject(self.value);
                }
            });
        };

        Promise.prototype.catch = function(onRejected) {
            return this.then(null, onRejected);
        };

        Promise.prototype.finally = function(onFinally) {
            return this.then(
                function(value) { return Promise.resolve(onFinally()).then(function() { return value; }); },
                function(reason) { return Promise.resolve(onFinally()).then(function() { throw reason; }); }
            );
        };

        Promise.resolve = function(value) {
            return new Promise(function(resolve) { resolve(value); });
        };

        Promise.reject = function(reason) {
            return new Promise(function(resolve, reject) { reject(reason); });
        };
        
        Promise.all = function(promises) {
             return new Promise(function(resolve, reject) {
                 var results = [];
                 var completed = 0;
                 if (promises.length === 0) {
                     resolve(results);
                 } else {
                     for (var i = 0; i < promises.length; i++) {
                         (function(index) {
                             Promise.resolve(promises[index]).then(function(value) {
                                 results[index] = value;
                                 completed++;
                                 if (completed === promises.length) {
                                     resolve(results);
                                 }
                             }, reject);
                         })(i);
                     }
                 }
             });
         };

         Promise.race = function(promises) {
             return new Promise(function(resolve, reject) {
                 for (var i = 0; i < promises.length; i++) {
                     Promise.resolve(promises[i]).then(resolve, reject);
                 }
             });
         };

        global.Promise = Promise;
    })(this);
}

// Window / Global stubs
if (typeof window === 'undefined') {
    this.window = this;
}

// Global error handler to prevent some crashes from propagating
window.onerror = function(msg, url, line, col, error) {
    // Return true to prevent the firing of the default event handler
    // console.log("Suppressed JS Error: " + msg); 
    return true; 
};

if (typeof window.matchMedia === 'undefined') {
    window.matchMedia = function(query) {
        return {
            matches: false,
            media: query,
            onchange: null,
            addListener: function() {},
            removeListener: function() {},
            addEventListener: function() {},
            removeEventListener: function() {},
            dispatchEvent: function() { return false; }
        };
    };
}

if (typeof window.scrollTo === 'undefined') {
    window.scrollTo = function(x, y) {};
}

if (typeof window.scrollBy === 'undefined') {
    window.scrollBy = function(x, y) {};
}

if (typeof window.addEventListener === 'undefined') {
    window.addEventListener = function(event, callback) {};
}

if (typeof window.removeEventListener === 'undefined') {
    window.removeEventListener = function(event, callback) {};
}

if (typeof document.addEventListener === 'undefined') {
    document.addEventListener = function(event, callback) {};
}

if (typeof document.removeEventListener === 'undefined') {
    document.removeEventListener = function(event, callback) {};
}

// Storage Stubs (localStorage, sessionStorage)
var _createStorageStub = function() {
    var data = {};
    return {
        getItem: function(key) { return data[key] || null; },
        setItem: function(key, value) { data[key] = String(value); },
        removeItem: function(key) { delete data[key]; },
        clear: function() { data = {}; },
        key: function(index) { return Object.keys(data)[index] || null; },
        get length() { return Object.keys(data).length; }
    };
};

if (typeof localStorage === 'undefined') {
    this.localStorage = _createStorageStub();
}
if (typeof sessionStorage === 'undefined') {
    this.sessionStorage = _createStorageStub();
}

// Navigator Stub
if (typeof navigator === 'undefined') {
    this.navigator = {
        userAgent: 'NetSurf/3.11 (Windows; x64) Duktape/2.6.0',
        platform: 'Win32',
        language: 'en-US',
        onLine: true,
        cookieEnabled: true,
        javaEnabled: function() { return false; },
        sendBeacon: function() { return false; }
    };
}

// History Stub
if (typeof history === 'undefined') {
    this.history = {
        length: 1,
        state: null,
        back: function() {},
        forward: function() {},
        go: function() {},
        pushState: function(state, title, url) {},
        replaceState: function(state, title, url) {}
    };
}

// Screen Stub
if (typeof screen === 'undefined') {
    this.screen = {
        width: 1920,
        height: 1080,
        availWidth: 1920,
        availHeight: 1040,
        colorDepth: 24,
        pixelDepth: 24
    };
}

// Location Stub (if missing)
if (typeof location === 'undefined') {
    this.location = {
        href: '',
        protocol: 'http:',
        host: '',
        hostname: '',
        port: '',
        pathname: '/',
        search: '',
        hash: '',
        reload: function() {},
        replace: function() {},
        assign: function() {}
    };
}

// MutationObserver Stub
if (typeof MutationObserver === 'undefined') {
    this.MutationObserver = function(callback) {
        this.observe = function(target, options) {};
        this.disconnect = function() {};
        this.takeRecords = function() { return []; };
    };
}

// Fetch Stub (Basic)
if (typeof fetch === 'undefined') {
    this.fetch = function(url, options) {
        return new Promise(function(resolve, reject) {
            // Minimal fetch polyfill using XMLHttpRequest if available, else stub
            if (typeof XMLHttpRequest !== 'undefined') {
                var xhr = new XMLHttpRequest();
                xhr.open(options && options.method || 'GET', url);
                xhr.onload = function() {
                    resolve({
                        ok: xhr.status >= 200 && xhr.status < 300,
                        status: xhr.status,
                        statusText: xhr.statusText,
                        text: function() { return Promise.resolve(xhr.responseText); },
                        json: function() { 
                            try { return Promise.resolve(JSON.parse(xhr.responseText)); } 
                            catch(e) { return Promise.reject(e); } 
                        },
                        headers: { get: function(name) { return null; } }
                    });
                };
                xhr.onerror = function() {
                    reject(new TypeError('Network request failed'));
                };
                xhr.send(options && options.body || null);
            } else {
                console.warn('fetch called but no network transport available');
                resolve({
                    ok: false,
                    status: 0,
                    text: function() { return Promise.resolve(''); },
                    json: function() { return Promise.resolve({}); }
                });
            }
        });
    };
}

// Image Stub
if (typeof Image === 'undefined') {
    this.Image = function(width, height) {
        var img = document.createElement('img');
        if (width) img.width = width;
        if (height) img.height = height;
        return img;
    };
}

// XMLHttpRequest Stub
if (typeof XMLHttpRequest === 'undefined') {
    this.XMLHttpRequest = function() {
        this.open = function() {};
        this.send = function() {};
        this.setRequestHeader = function() {};
        this.getAllResponseHeaders = function() { return ''; };
        this.getResponseHeader = function() { return null; };
        this.abort = function() {};
        this.addEventListener = function() {};
        this.removeEventListener = function() {};
        this.onreadystatechange = null;
        this.readyState = 0;
        this.status = 0;
        this.statusText = '';
        this.responseText = '';
        this.responseXML = null;
    };
}

// Element stubs
if (typeof Element !== 'undefined') {
    if (!Element.prototype.matches) {
        Element.prototype.matches = 
            Element.prototype.matchesSelector || 
            Element.prototype.mozMatchesSelector ||
            Element.prototype.msMatchesSelector || 
            Element.prototype.oMatchesSelector || 
            Element.prototype.webkitMatchesSelector ||
            function(s) {
                var matches = (this.document || this.ownerDocument).querySelectorAll(s),
                    i = matches.length;
                while (--i >= 0 && matches.item(i) !== this) {}
                return i > -1;
            };
    }

    if (!Element.prototype.closest) {
        Element.prototype.closest = function(s) {
            var el = this;
            if (!document.documentElement.contains(el)) return null;
            do {
                if (el.matches(s)) return el;
                el = el.parentElement || el.parentNode;
            } while (el !== null && el.nodeType === 1); 
            return null;
        };
    }
}

// CustomEvent polyfill
if (typeof CustomEvent === 'undefined') {
    (function() {
        function CustomEvent(event, params) {
            params = params || { bubbles: false, cancelable: false, detail: undefined };
            var evt = document.createEvent('CustomEvent');
            evt.initCustomEvent(event, params.bubbles, params.cancelable, params.detail);
            return evt;
        }
        CustomEvent.prototype = window.Event.prototype;
        window.CustomEvent = CustomEvent;
    })();
}

// URL Stub
if (typeof URL === 'undefined') {
    this.URL = function(url, base) {
        this.href = url;
        this.protocol = '';
        this.host = '';
        this.hostname = '';
        this.port = '';
        this.pathname = '';
        this.search = '';
        this.hash = '';
        this.origin = '';
    };
    this.URL.createObjectURL = function() { return ''; };
    this.URL.revokeObjectURL = function() { };
}

// URLSearchParams Stub
if (typeof URLSearchParams === 'undefined') {
    this.URLSearchParams = function(init) {
        this.params = {};
    };
    this.URLSearchParams.prototype.get = function(name) { return null; };
    this.URLSearchParams.prototype.getAll = function(name) { return []; };
    this.URLSearchParams.prototype.has = function(name) { return false; };
    this.URLSearchParams.prototype.set = function(name, value) {};
    this.URLSearchParams.prototype.append = function(name, value) {};
    this.URLSearchParams.prototype.delete = function(name) {};
    this.URLSearchParams.prototype.toString = function() { return ''; };
}

// IntersectionObserver Stub
if (typeof IntersectionObserver === 'undefined') {
    this.IntersectionObserver = function(callback, options) {
        this.observe = function(target) {};
        this.unobserve = function(target) {};
        this.disconnect = function() {};
        this.takeRecords = function() { return []; };
    };
}

// ResizeObserver Stub
if (typeof ResizeObserver === 'undefined') {
    this.ResizeObserver = function(callback) {
        this.observe = function(target) {};
        this.unobserve = function(target) {};
        this.disconnect = function() {};
    };
}