(function() {
  function a(d) {
    d = d.split("-")[0];
    for (d = d.split(".").slice(0, 3); 3 > d.length;) {
      d.push("00");
    }
    d = d.map(e => e.padStart(2, "0"));
    return d.join("");
  }
  var b = "undefined" !== typeof process && process.versions?.node ? a(process.versions.node) : 2147483647;
  if (180300 > b) {
    throw Error(`This emscripten-generated code requires node v${"18.3.0"} (detected v${[b / 10000 | 0, (b / 100 | 0) % 100, b % 100].join(".")})`);
  }
  if (b = "undefined" !== typeof navigator && navigator.userAgent) {
    var c = b.includes("Safari/") && !b.includes("Chrome/") && b.match(/Version\/(\d+\.?\d*\.?\d*)/) ? a(b.match(/Version\/(\d+\.?\d*\.?\d*)/)[1]) : 2147483647;
    if (150000 > c) {
      throw Error(`This emscripten-generated code requires Safari v${"15.0.0"} (detected v${c})`);
    }
    c = b.match(/Firefox\/(\d+(?:\.\d+)?)/) ? parseFloat(b.match(/Firefox\/(\d+(?:\.\d+)?)/)[1]) : 2147483647;
    if (79 > c) {
      throw Error(`This emscripten-generated code requires Firefox v79 (detected v${c})`);
    }
    b = b.match(/Chrome\/(\d+(?:\.\d+)?)/) ? parseFloat(b.match(/Chrome\/(\d+(?:\.\d+)?)/)[1]) : 2147483647;
    if (85 > b) {
      throw Error(`This emscripten-generated code requires Chrome v85 (detected v${b})`);
    }
  }
})();
var h;
h ||= typeof Module != 'undefined' ? Module : {};
var aa = !!globalThis.window, ca = !!globalThis.WorkerGlobalScope, da = globalThis.process?.versions?.node && "renderer" != globalThis.process?.type, fa = !aa && !da && !ca;
h.expectedDataFileDownloads || (h.expectedDataFileDownloads = 0);
h.expectedDataFileDownloads++;
(() => {
  var a = "undefined" != typeof ENVIRONMENT_IS_WASM_WORKER && ENVIRONMENT_IS_WASM_WORKER;
  if (!("undefined" != typeof ENVIRONMENT_IS_PTHREAD && ENVIRONMENT_IS_PTHREAD || a)) {
    var b = globalThis.process && globalThis.process.versions && globalThis.process.versions.node && "renderer" != globalThis.process.type;
    (async function(c) {
      async function d(r, t) {
        if (b) {
          var v = require("fs").readFileSync(r);
          return (new Uint8Array(v)).buffer;
        }
        h.dataFileDownloads || (h.dataFileDownloads = {});
        try {
          var u = await fetch(r);
        } catch (U) {
          throw Error(`Network Error: ${r}`, {e:U});
        }
        if (!u.ok) {
          throw Error(`${u.status}: ${u.url}`);
        }
        v = [];
        t = Number(u.headers.get("Content-Length") || t);
        let H = 0;
        h.setStatus && h.setStatus("Downloading data...");
        for (u = u.body.getReader();;) {
          var {done:N, value:S} = await u.read();
          if (N) {
            break;
          }
          v.push(S);
          H += S.length;
          h.dataFileDownloads[r] = {loaded:H, total:t};
          let U = 0, ea = 0;
          for (var ba of Object.values(h.dataFileDownloads)) {
            U += ba.loaded, ea += ba.total;
          }
          h.setStatus && h.setStatus(`Downloading data... (${U}/${ea})`);
        }
        r = new Uint8Array(v.map(U => U.length).reduce((U, ea) => U + ea, 0));
        ba = 0;
        for (const U of v) {
          r.set(U, ba), ba += U.length;
        }
        return r.buffer;
      }
      async function e(r) {
        r.FS_createPath("/", "assets", !0, !0);
        r.FS_createPath("/assets", "fonts", !0, !0);
        for (var t of c.files) {
          r.addRunDependency(`fp ${t.filename}`);
        }
        r.addRunDependency("datafile_web/aslan.data");
        r.preloadResults || (r.preloadResults = {});
        r.preloadResults["web/aslan.data"] = {md:!1};
        n ||= await l;
        (async function(v) {
          if (!v) {
            throw Error("Loading data file failed.");
          }
          if (v.constructor.name !== ArrayBuffer.name) {
            throw Error("bad input to processPackageData " + v.constructor.name);
          }
          v = new Uint8Array(v);
          for (var u of c.files) {
            var H = u.filename;
            r.FS_createDataFile(H, null, v.subarray(u.start, u.end), !0, !0, !0);
            r.removeRunDependency(`fp ${H}`);
          }
          r.removeRunDependency("datafile_web/aslan.data");
        })(n);
      }
      "object" === typeof window ? window.encodeURIComponent(window.location.pathname.substring(0, window.location.pathname.lastIndexOf("/")) + "/") : "undefined" === typeof process && "undefined" !== typeof location && encodeURIComponent(location.pathname.substring(0, location.pathname.lastIndexOf("/")) + "/");
      var f = h.locateFile ? h.locateFile("aslan.data", "") : "aslan.data", g = c.remote_package_size, l, n = h.getPreloadedPackage && h.getPreloadedPackage(f, g);
      n || (l = d(f, g));
      h.calledRun ? e(h) : (h.preRun || (h.preRun = []), h.preRun.push(e));
    })({files:[{filename:"/assets/fonts/Inter-Regular.ttf", start:0, end:342680}, {filename:"/assets/fonts/Inter_28pt-Bold.ttf", start:342680, end:686652}, {filename:"/assets/fonts/Inter_28pt-ExtraBold.ttf", start:686652, end:1031344}, {filename:"/assets/remixicon.ttf", start:1031344, end:1601320}], remote_package_size:1601320});
  }
})();
if ("undefined" != typeof ENVIRONMENT_IS_WASM_WORKER && ENVIRONMENT_IS_WASM_WORKER || "undefined" != typeof ENVIRONMENT_IS_PTHREAD && ENVIRONMENT_IS_PTHREAD || "undefined" != typeof ENVIRONMENT_IS_AUDIO_WORKLET && ENVIRONMENT_IS_AUDIO_WORKLET) {
  h.preRun = [];
}
var ha = h.preRun.slice();
if (!h.preRun) {
  throw "Module.preRun should exist because file support used it; did a pre-js delete it?";
}
ha.forEach(a => {
  if (0 > h.preRun.indexOf(a)) {
    throw "All preRun tasks that exist before user pre-js code should remain after; did you replace Module or modify Module.preRun?";
  }
});
var ia = [], ja = "./this.program", ka = (a, b) => {
  throw b;
}, la = globalThis.document?.currentScript?.src;
"undefined" != typeof __filename ? la = __filename : ca && (la = self.location.href);
var ma = "", na, oa;
if (da) {
  if (!globalThis.process?.versions?.node || "renderer" == globalThis.process?.type) {
    throw Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
  }
  var fs = require("node:fs");
  ma = __dirname + "/";
  oa = a => {
    a = pa(a) ? new URL(a) : a;
    a = fs.readFileSync(a);
    k(Buffer.isBuffer(a));
    return a;
  };
  na = async a => {
    a = pa(a) ? new URL(a) : a;
    a = fs.readFileSync(a, void 0);
    k(Buffer.isBuffer(a));
    return a;
  };
  1 < process.argv.length && (ja = process.argv[1].replace(/\\/g, "/"));
  ia = process.argv.slice(2);
  "undefined" != typeof module && (module.exports = h);
  ka = (a, b) => {
    process.exitCode = a;
    throw b;
  };
} else if (!fa) {
  if (aa || ca) {
    try {
      ma = (new URL(".", la)).href;
    } catch {
    }
    if (!globalThis.window && !globalThis.WorkerGlobalScope) {
      throw Error("not compiled for this environment (did you build to HTML and try to run it not on the web, or set ENVIRONMENT to something - like node - and run it someplace else - like on the web?)");
    }
    ca && (oa = a => {
      var b = new XMLHttpRequest();
      b.open("GET", a, !1);
      b.responseType = "arraybuffer";
      b.send(null);
      return new Uint8Array(b.response);
    });
    na = async a => {
      if (pa(a)) {
        return new Promise((c, d) => {
          var e = new XMLHttpRequest();
          e.open("GET", a, !0);
          e.responseType = "arraybuffer";
          e.onload = () => {
            200 == e.status || 0 == e.status && e.response ? c(e.response) : d(e.status);
          };
          e.onerror = d;
          e.send(null);
        });
      }
      var b = await fetch(a, {credentials:"same-origin"});
      if (b.ok) {
        return b.arrayBuffer();
      }
      throw Error(b.status + " : " + b.url);
    };
  } else {
    throw Error("environment detection error");
  }
}
var qa = console.log.bind(console), m = console.error.bind(console);
k(!fa, "shell environment detected but not enabled at build time.  Add `shell` to `-sENVIRONMENT` to enable.");
var ra;
globalThis.WebAssembly || m("no native wasm support detected");
var sa = !1, ta;
function k(a, b) {
  a || p("Assertion failed" + (b ? ": " + b : ""));
}
var pa = a => a.startsWith("file://");
function ua() {
  var a = va();
  k(0 == (a & 3));
  0 == a && (a += 4);
  q[a >> 2] = 34821223;
  q[a + 4 >> 2] = 2310721022;
  q[0] = 1668509029;
}
function wa() {
  if (!sa) {
    var a = va();
    0 == a && (a += 4);
    var b = q[a >> 2], c = q[a + 4 >> 2];
    34821223 == b && 2310721022 == c || p(`Stack overflow! Stack cookie has been overwritten at ${xa(a)}, expected hex dwords 0x89BACDFE and 0x2135467, but received ${xa(c)} ${xa(b)}`);
    1668509029 != q[0] && p("Runtime error: The application has corrupted its heap memory area (address zero)!");
  }
}
var ya = new Int16Array(1), za = new Int8Array(ya.buffer);
ya[0] = 25459;
115 === za[0] && 99 === za[1] || p("Runtime error: expected the system to be little-endian! (Run with -sSUPPORT_BIG_ENDIAN to bypass)");
function Aa(a) {
  Object.getOwnPropertyDescriptor(h, a) || Object.defineProperty(h, a, {configurable:!0, set() {
    p(`Attempt to set \`Module.${a}\` after it has already been processed.  This can happen, for example, when code is injected via '--post-js' rather than '--pre-js'`);
  }});
}
function w(a) {
  return () => k(!1, `call to '${a}' via reference taken before Wasm module initialization`);
}
function Ba(a) {
  Object.getOwnPropertyDescriptor(h, a) && p(`\`Module.${a}\` was supplied but \`${a}\` not included in INCOMING_MODULE_JS_API`);
}
function Ca(a) {
  return "FS_createPath" === a || "FS_createDataFile" === a || "FS_createPreloadedFile" === a || "FS_preloadFile" === a || "FS_unlink" === a || "addRunDependency" === a || "FS_createLazyFile" === a || "FS_createDevice" === a || "removeRunDependency" === a;
}
function Da(a, b) {
  Object.getOwnPropertyDescriptor(globalThis, a) || Object.defineProperty(globalThis, a, {configurable:!0, get() {
    b();
  }});
}
function Ea(a, b) {
  Da(a, () => {
    Fa(`\`${a}\` is no longer defined by emscripten. ${b}`);
  });
}
Ea("buffer", "Please use HEAP8.buffer or wasmMemory.buffer");
Ea("asm", "Please use wasmExports instead");
function Ga(a) {
  Object.getOwnPropertyDescriptor(h, a) || Object.defineProperty(h, a, {configurable:!0, get() {
    var b = `'${a}' was not exported. add it to EXPORTED_RUNTIME_METHODS (see the Emscripten FAQ)`;
    Ca(a) && (b += ". Alternatively, forcing filesystem support (-sFORCE_FILESYSTEM) can export this for you");
    p(b);
  }});
}
var Ha = !1;
function Ia() {
  var a = Ja.buffer;
  x = new Int8Array(a);
  Ka = new Int16Array(a);
  y = new Uint8Array(a);
  La = new Uint16Array(a);
  z = new Int32Array(a);
  q = new Uint32Array(a);
  A = new Float32Array(a);
  Ma = new Float64Array(a);
  Na = new BigInt64Array(a);
  new BigUint64Array(a);
}
k(globalThis.Int32Array && globalThis.Float64Array && Int32Array.prototype.subarray && Int32Array.prototype.set, "JS engine does not provide full typed array support");
function p(a) {
  h.onAbort?.(a);
  a = `Aborted(${a})`;
  m(a);
  sa = !0;
  0 <= a.search(/RuntimeError: [Uu]nreachable/) && (a += '. "unreachable" may be due to ASYNCIFY_STACK_SIZE not being large enough (try increasing it)');
  throw new WebAssembly.RuntimeError(a);
}
function B(a, b) {
  return (...c) => {
    k(Ha, `native function \`${a}\` called before runtime initialization`);
    var d = Oa[a];
    k(d, `exported native function \`${a}\` not found`);
    k(c.length <= b, `native function \`${a}\` called with ${c.length} args but expects ${b}`);
    return d(...c);
  };
}
var Pa;
async function Qa(a) {
  if (!ra) {
    try {
      var b = await na(a);
      return new Uint8Array(b);
    } catch {
    }
  }
  if (a == Pa && ra) {
    a = new Uint8Array(ra);
  } else {
    if (oa) {
      a = oa(a);
    } else {
      throw "both async and sync fetching of the wasm failed";
    }
  }
  return a;
}
async function Ra(a, b) {
  try {
    var c = await Qa(a);
    return await WebAssembly.instantiate(c, b);
  } catch (d) {
    m(`failed to asynchronously prepare wasm: ${d}`), pa(a) && m(`warning: Loading from a file URI (${a}) is not supported in most browsers. See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing`), p(d);
  }
}
async function Sa(a) {
  var b = Pa;
  if (!ra && !pa(b) && !da) {
    try {
      var c = fetch(b, {credentials:"same-origin"});
      return await WebAssembly.instantiateStreaming(c, a);
    } catch (d) {
      m(`wasm streaming compile failed: ${d}`), m("falling back to ArrayBuffer instantiation");
    }
  }
  return Ra(b, a);
}
class Ta {
  name="ExitStatus";
  constructor(a) {
    this.message = `Program terminated with exit(${a})`;
    this.status = a;
  }
}
var Ka, z, Na, x, A, Ma, La, q, y, Ua = a => {
  for (; 0 < a.length;) {
    a.shift()(h);
  }
}, Va = [], Wa = [], Xa = () => {
  var a = h.preRun.shift();
  Wa.push(a);
}, Ya = 0, Za = null, $a = {}, ab = null, bb = a => {
  Ya--;
  h.monitorRunDependencies?.(Ya);
  k(a, "removeRunDependency requires an ID");
  k($a[a]);
  delete $a[a];
  0 == Ya && (null !== ab && (clearInterval(ab), ab = null), Za && (a = Za, Za = null, a()));
}, cb = a => {
  Ya++;
  h.monitorRunDependencies?.(Ya);
  k(a, "addRunDependency requires an ID");
  k(!$a[a]);
  $a[a] = 1;
  null === ab && globalThis.setInterval && (ab = setInterval(() => {
    if (sa) {
      clearInterval(ab), ab = null;
    } else {
      var b = !1, c;
      for (c in $a) {
        b || (b = !0, m("still waiting on run dependencies:")), m(`dependency: ${c}`);
      }
      b && m("(end of list)");
    }
  }, 10000), ab.unref?.());
}, C = {}, db = (a, b = []) => {
  k(a, "null function pointer in dynCall");
  k(!0, "async dynCall is not supported in this mode");
  var c = "vp".replace(/p/g, "i");
  k(c in C, `bad function pointer type - sig is not in dynCalls: '${c}'`);
  b?.length ? k(b.length === c.length - 1) : k(1 == c.length);
  (0,C[c])(a, ...b);
}, eb = !0;
function xa(a) {
  k("number" === typeof a, `ptrToString expects a number, got ${typeof a}`);
  return "0x" + (a >>> 0).toString(16).padStart(8, "0");
}
function fb(a, b) {
  var c = "float";
  c.endsWith("*") && (c = "*");
  switch(c) {
    case "i1":
      x[a] = b;
      break;
    case "i8":
      x[a] = b;
      break;
    case "i16":
      Ka[a >> 1] = b;
      break;
    case "i32":
      z[a >> 2] = b;
      break;
    case "i64":
      Na[a >> 3] = BigInt(b);
      break;
    case "float":
      A[a >> 2] = b;
      break;
    case "double":
      Ma[a >> 3] = b;
      break;
    case "*":
      q[a >> 2] = b;
      break;
    default:
      p(`invalid type for setValue: ${c}`);
  }
}
var Fa = a => {
  Fa.kb || (Fa.kb = {});
  Fa.kb[a] || (Fa.kb[a] = 1, da && (a = "warning: " + a), m(a));
}, gb = globalThis.TextDecoder && new TextDecoder(), hb = (a, b = 0, c, d) => {
  var e = b;
  c = e + c;
  if (d) {
    d = c;
  } else {
    for (; a[e] && !(e >= c);) {
      ++e;
    }
    d = e;
  }
  if (16 < d - b && a.buffer && gb) {
    return gb.decode(a.subarray(b, d));
  }
  for (e = ""; b < d;) {
    if (c = a[b++], c & 128) {
      var f = a[b++] & 63;
      if (192 == (c & 224)) {
        e += String.fromCharCode((c & 31) << 6 | f);
      } else {
        var g = a[b++] & 63;
        224 == (c & 240) ? c = (c & 15) << 12 | f << 6 | g : (240 != (c & 248) && Fa(`Invalid UTF-8 leading byte ${xa(c)} encountered when deserializing a UTF-8 string in wasm memory to a JS string!`), c = (c & 7) << 18 | f << 12 | g << 6 | a[b++] & 63);
        65536 > c ? e += String.fromCharCode(c) : (c -= 65536, e += String.fromCharCode(55296 | c >> 10, 56320 | c & 1023));
      }
    } else {
      e += String.fromCharCode(c);
    }
  }
  return e;
}, D = (a, b, c) => {
  k("number" == typeof a, `UTF8ToString expects a number (got ${typeof a})`);
  return a ? hb(y, a, b, c) : "";
}, jb = () => {
  k(void 0 != ib);
  var a = z[+ib >> 2];
  ib += 4;
  return a;
}, kb = (a, b) => {
  for (var c = 0, d = a.length - 1; 0 <= d; d--) {
    var e = a[d];
    "." === e ? a.splice(d, 1) : ".." === e ? (a.splice(d, 1), c++) : c && (a.splice(d, 1), c--);
  }
  if (b) {
    for (; c; c--) {
      a.unshift("..");
    }
  }
  return a;
}, lb = a => {
  var b = "/" === a.charAt(0), c = "/" === a.slice(-1);
  (a = kb(a.split("/").filter(d => !!d), !b).join("/")) || b || (a = ".");
  a && c && (a += "/");
  return (b ? "/" : "") + a;
}, mb = a => {
  var b = /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/.exec(a).slice(1);
  a = b[0];
  b = b[1];
  if (!a && !b) {
    return ".";
  }
  b &&= b.slice(0, -1);
  return a + b;
}, nb = a => a && a.match(/([^\/]+|\/)\/*$/)[1], ob = (a, b) => lb(a + "/" + b), pb = () => {
  if (da) {
    var a = require("node:crypto");
    return b => a.randomFillSync(b);
  }
  return b => (crypto.getRandomValues(b), 0);
}, qb = a => (qb = pb())(a), rb = (...a) => {
  for (var b = "", c = !1, d = a.length - 1; -1 <= d && !c; d--) {
    c = 0 <= d ? a[d] : E.cwd();
    if ("string" != typeof c) {
      throw new TypeError("Arguments to path.resolve must be strings");
    }
    if (!c) {
      return "";
    }
    b = c + "/" + b;
    c = "/" === c.charAt(0);
  }
  b = kb(b.split("/").filter(e => !!e), !c).join("/");
  return (c ? "/" : "") + b || ".";
}, sb = (a, b) => {
  function c(g) {
    for (var l = 0; l < g.length && "" === g[l]; l++) {
    }
    for (var n = g.length - 1; 0 <= n && "" === g[n]; n--) {
    }
    return l > n ? [] : g.slice(l, n - l + 1);
  }
  a = rb(a).slice(1);
  b = rb(b).slice(1);
  a = c(a.split("/"));
  b = c(b.split("/"));
  for (var d = Math.min(a.length, b.length), e = d, f = 0; f < d; f++) {
    if (a[f] !== b[f]) {
      e = f;
      break;
    }
  }
  d = [];
  for (f = e; f < a.length; f++) {
    d.push("..");
  }
  d = d.concat(b.slice(e));
  return d.join("/");
}, tb = [], ub = a => {
  for (var b = 0, c = 0; c < a.length; ++c) {
    var d = a.charCodeAt(c);
    127 >= d ? b++ : 2047 >= d ? b += 2 : 55296 <= d && 57343 >= d ? (b += 4, ++c) : b += 3;
  }
  return b;
}, vb = (a, b, c, d) => {
  k("string" === typeof a, `stringToUTF8Array expects a string (got ${typeof a})`);
  if (!(0 < d)) {
    return 0;
  }
  var e = c;
  d = c + d - 1;
  for (var f = 0; f < a.length; ++f) {
    var g = a.codePointAt(f);
    if (127 >= g) {
      if (c >= d) {
        break;
      }
      b[c++] = g;
    } else if (2047 >= g) {
      if (c + 1 >= d) {
        break;
      }
      b[c++] = 192 | g >> 6;
      b[c++] = 128 | g & 63;
    } else if (65535 >= g) {
      if (c + 2 >= d) {
        break;
      }
      b[c++] = 224 | g >> 12;
      b[c++] = 128 | g >> 6 & 63;
      b[c++] = 128 | g & 63;
    } else {
      if (c + 3 >= d) {
        break;
      }
      1114111 < g && Fa(`Invalid Unicode code point ${xa(g)} encountered when serializing a JS string to a UTF-8 string in wasm memory! (Valid unicode code points should be in range 0-0x10FFFF).`);
      b[c++] = 240 | g >> 18;
      b[c++] = 128 | g >> 12 & 63;
      b[c++] = 128 | g >> 6 & 63;
      b[c++] = 128 | g & 63;
      f++;
    }
  }
  b[c] = 0;
  return c - e;
}, wb = a => {
  var b = Array(ub(a) + 1);
  a = vb(a, b, 0, b.length);
  b.length = a;
  return b;
}, xb = [];
function yb(a, b) {
  xb[a] = {input:[], output:[], Z:b};
  zb(a, Ab);
}
var Ab = {open(a) {
  var b = xb[a.node.rdev];
  if (!b) {
    throw new E.g(43);
  }
  a.tty = b;
  a.seekable = !1;
}, close(a) {
  a.tty.Z.fsync(a.tty);
}, fsync(a) {
  a.tty.Z.fsync(a.tty);
}, read(a, b, c, d) {
  if (!a.tty || !a.tty.Z.Ab) {
    throw new E.g(60);
  }
  for (var e = 0, f = 0; f < d; f++) {
    try {
      var g = a.tty.Z.Ab(a.tty);
    } catch (l) {
      throw new E.g(29);
    }
    if (void 0 === g && 0 === e) {
      throw new E.g(6);
    }
    if (null === g || void 0 === g) {
      break;
    }
    e++;
    b[c + f] = g;
  }
  e && (a.node.atime = Date.now());
  return e;
}, write(a, b, c, d) {
  if (!a.tty || !a.tty.Z.ib) {
    throw new E.g(60);
  }
  try {
    for (var e = 0; e < d; e++) {
      a.tty.Z.ib(a.tty, b[c + e]);
    }
  } catch (f) {
    throw new E.g(29);
  }
  d && (a.node.mtime = a.node.ctime = Date.now());
  return e;
}}, Bb = {Ab() {
  a: {
    if (!tb.length) {
      var a = null;
      if (da) {
        var b = Buffer.alloc(256), c = 0, d = process.stdin.fd;
        try {
          c = fs.readSync(d, b, 0, 256);
        } catch (e) {
          if (e.toString().includes("EOF")) {
            c = 0;
          } else {
            throw e;
          }
        }
        0 < c && (a = b.slice(0, c).toString("utf-8"));
      } else {
        globalThis.window?.prompt && (a = window.prompt("Input: "), null !== a && (a += "\n"));
      }
      if (!a) {
        a = null;
        break a;
      }
      tb = wb(a);
    }
    a = tb.shift();
  }
  return a;
}, ib(a, b) {
  null === b || 10 === b ? (qa(hb(a.output)), a.output = []) : 0 != b && a.output.push(b);
}, fsync(a) {
  0 < a.output?.length && (qa(hb(a.output)), a.output = []);
}, rc() {
  return {Xc:25856, Zc:5, Wc:191, Yc:35387, Vc:[3, 28, 127, 21, 4, 0, 1, 0, 17, 19, 26, 0, 18, 15, 23, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]};
}, sc() {
  return 0;
}, tc() {
  return [24, 80];
}}, Cb = {ib(a, b) {
  null === b || 10 === b ? (m(hb(a.output)), a.output = []) : 0 != b && a.output.push(b);
}, fsync(a) {
  0 < a.output?.length && (m(hb(a.output)), a.output = []);
}}, Db = () => {
  p("internal error: mmapAlloc called but `emscripten_builtin_memalign` native symbol not exported");
};
function Eb(a) {
  k(E.isFile(a.mode), "getFileDataAsTypedArray called on non-file");
  return a.l.subarray(0, a.A);
}
var F = {U:null, m() {
  return F.createNode(null, "/", 16895, 0);
}, createNode(a, b, c, d) {
  if (24576 === (c & 61440) || E.isFIFO(c)) {
    throw new E.g(63);
  }
  F.U || (F.U = {dir:{node:{L:F.h.L, R:F.h.R, lookup:F.h.lookup, N:F.h.N, rename:F.h.rename, unlink:F.h.unlink, rmdir:F.h.rmdir, readdir:F.h.readdir, symlink:F.h.symlink}, stream:{D:F.j.D}}, file:{node:{L:F.h.L, R:F.h.R}, stream:{D:F.j.D, read:F.j.read, write:F.j.write, na:F.j.na, wa:F.j.wa}}, link:{node:{L:F.h.L, R:F.h.R, readlink:F.h.readlink}, stream:{}}, vb:{node:{L:F.h.L, R:F.h.R}, stream:E.Tb}});
  c = E.createNode(a, b, c, d);
  G(c.mode) ? (c.h = F.U.dir.node, c.j = F.U.dir.stream, c.l = {}) : E.isFile(c.mode) ? (c.h = F.U.file.node, c.j = F.U.file.stream, c.A = 0, c.l = F.ac ?? (F.ac = new Uint8Array(0))) : 40960 === (c.mode & 61440) ? (c.h = F.U.link.node, c.j = F.U.link.stream) : 8192 === (c.mode & 61440) && (c.h = F.U.vb.node, c.j = F.U.vb.stream);
  c.atime = c.mtime = c.ctime = Date.now();
  a && (a.l[b] = c, a.atime = a.mtime = a.ctime = c.atime);
  return c;
}, h:{L(a) {
  var b = {};
  b.dev = 8192 === (a.mode & 61440) ? a.id : 1;
  b.ino = a.id;
  b.mode = a.mode;
  b.nlink = 1;
  b.uid = 0;
  b.gid = 0;
  b.rdev = a.rdev;
  G(a.mode) ? b.size = 4096 : E.isFile(a.mode) ? b.size = a.A : 40960 === (a.mode & 61440) ? b.size = a.link.length : b.size = 0;
  b.atime = new Date(a.atime);
  b.mtime = new Date(a.mtime);
  b.ctime = new Date(a.ctime);
  b.blksize = 4096;
  b.blocks = Math.ceil(b.size / b.blksize);
  return b;
}, R(a, b) {
  for (var c of ["mode", "atime", "mtime", "ctime"]) {
    null != b[c] && (a[c] = b[c]);
  }
  void 0 !== b.size && (b = b.size, a.A != b && (c = a.l, a.l = new Uint8Array(b), a.l.set(c.subarray(0, Math.min(b, a.A))), a.A = b));
}, lookup() {
  throw new E.g(44);
}, N(a, b, c, d) {
  return F.createNode(a, b, c, d);
}, rename(a, b, c) {
  try {
    var d = Fb(b, c);
  } catch (f) {
  }
  if (d) {
    if (G(a.mode)) {
      for (var e in d.l) {
        throw new E.g(55);
      }
    }
    Gb(d);
  }
  delete a.parent.l[a.name];
  b.l[c] = a;
  a.name = c;
  b.ctime = b.mtime = a.parent.ctime = a.parent.mtime = Date.now();
}, unlink(a, b) {
  delete a.l[b];
  a.ctime = a.mtime = Date.now();
}, rmdir(a, b) {
  var c = Fb(a, b), d;
  for (d in c.l) {
    throw new E.g(55);
  }
  delete a.l[b];
  a.ctime = a.mtime = Date.now();
}, readdir(a) {
  return [".", "..", ...Object.keys(a.l)];
}, symlink(a, b, c) {
  a = F.createNode(a, b, 41471, 0);
  a.link = c;
  return a;
}, readlink(a) {
  if (40960 !== (a.mode & 61440)) {
    throw new E.g(28);
  }
  return a.link;
}}, j:{read(a, b, c, d, e) {
  var f = a.node.l;
  if (e >= a.node.A) {
    return 0;
  }
  a = Math.min(a.node.A - e, d);
  k(0 <= a);
  b.set(f.subarray(e, e + a), c);
  return a;
}, write(a, b, c, d, e, f) {
  k(b.subarray, "FS.write expects a TypedArray");
  b.buffer === x.buffer && (f = !1);
  if (!d) {
    return 0;
  }
  a = a.node;
  a.mtime = a.ctime = Date.now();
  if (f) {
    k(0 === e, "canOwn must imply no weird position inside the file"), a.l = b.subarray(c, c + d), a.A = d;
  } else if (0 === a.A && 0 === e) {
    a.l = b.slice(c, c + d), a.A = d;
  } else {
    f = e + d;
    var g = a.l.length;
    g >= f || (f = Math.max(f, g * (1048576 > g ? 2.0 : 1.125) >>> 0), g && (f = Math.max(f, 256)), g = Eb(a), a.l = new Uint8Array(f), a.l.set(g));
    a.l.set(b.subarray(c, c + d), e);
    a.A = Math.max(a.A, e + d);
  }
  return d;
}, D(a, b, c) {
  1 === c ? b += a.position : 2 === c && E.isFile(a.node.mode) && (b += a.node.A);
  if (0 > b) {
    throw new E.g(28);
  }
  return b;
}, na(a, b, c, d, e) {
  if (!E.isFile(a.node.mode)) {
    throw new E.g(43);
  }
  a = a.node.l;
  if (e & 2 || a.buffer !== x.buffer) {
    d = !0;
    e = Db();
    if (!e) {
      throw new E.g(48);
    }
    if (a) {
      if (0 < c || c + b < a.length) {
        a.subarray ? a = a.subarray(c, c + b) : a = Array.prototype.slice.call(a, c, c + b);
      }
      x.set(a, e);
    }
  } else {
    d = !1, e = a.byteOffset;
  }
  return {oa:e, Rb:d};
}, wa(a, b, c, d) {
  F.j.write(a, b, 0, d, c, !1);
  return 0;
}}}, Hb = a => {
  "string" == typeof a && (a = wb(a));
  a.subarray || (a = new Uint8Array(a));
  return a;
}, Ib = (a, b) => {
  var c = 0;
  a && (c |= 365);
  b && (c |= 146);
  return c;
}, I = {Ea:{}, indexedDB:() => {
  k("undefined" != typeof indexedDB, "IDBFS used, but indexedDB not supported");
  return indexedDB;
}, Nb:21, ea:"FILE_DATA", pa:a => {
  function b() {
    "again" === a.fa ? c() : a.fa = 0;
  }
  function c() {
    a.fa = "idb";
    I.La(a, !1, b);
  }
  a.fa ? "idb" === a.fa && (a.fa = "again") : a.fa = setTimeout(c, 0);
}, m:a => {
  var b = F.m(a);
  if (a?.Hb?.Qc) {
    a.fa = 0;
    var c = b.h;
    b.h = {...b.h};
    b.h.N = (d, e, f, g) => {
      var l = c.N(d, e, f, g);
      l.h = b.h;
      l.oc = b.m;
      l.eb = l.j;
      l.j = {...l.j};
      l.j.write = (n, r, t, v, u, H) => {
        n.node.Db = !0;
        return l.eb.write(n, r, t, v, u, H);
      };
      l.j.close = n => {
        var r = n.node;
        r.Db && (I.pa(r.oc), r.Db = !1);
        if (r.eb.close) {
          return r.eb.close(n);
        }
      };
      I.pa(b.m);
      return l;
    };
    b.h.rmdir = (...d) => (I.pa(b.m), c.rmdir(...d));
    b.h.symlink = (...d) => (I.pa(b.m), c.symlink(...d));
    b.h.unlink = (...d) => (I.pa(b.m), c.unlink(...d));
    b.h.rename = (...d) => (I.pa(b.m), c.rename(...d));
  }
  return b;
}, La:(a, b, c) => {
  I.kc(a, (d, e) => {
    if (d) {
      return c(d);
    }
    I.lc(a, (f, g) => {
      if (f) {
        return c(f);
      }
      I.Fc(b ? g : e, b ? e : g, c);
    });
  });
}, Ec:() => {
  for (var a of Object.values(I.Ea)) {
    a.close();
  }
  I.Ea = {};
}, ic:(a, b) => {
  var c = I.Ea[a];
  if (c) {
    return b(null, c);
  }
  try {
    var d = I.indexedDB().open(a, I.Nb);
  } catch (e) {
    return b(e);
  }
  if (!d) {
    return b("Unable to connect to IndexedDB");
  }
  d.onupgradeneeded = e => {
    var f = e.target.result;
    e = e.target.transaction;
    var g;
    f.objectStoreNames.contains(I.ea) ? g = e.objectStore(I.ea) : g = f.createObjectStore(I.ea);
    g.indexNames.contains("timestamp") || g.createIndex("timestamp", "timestamp", {unique:!1});
  };
  d.onsuccess = () => {
    c = d.result;
    I.Ea[a] = c;
    b(null, c);
  };
  d.onerror = e => {
    b(e.target.error);
    e.preventDefault();
  };
}, kc:(a, b) => {
  function c(l) {
    return "." !== l && ".." !== l;
  }
  function d(l) {
    return n => lb(l + "/" + n);
  }
  var e = {};
  for (a = E.readdir(a.ua).filter(c).map(d(a.ua)); a.length;) {
    var f = a.pop();
    try {
      var g = E.lstat(f);
    } catch (l) {
      return b(l);
    }
    G(g.mode) && a.push(...E.readdir(f).filter(c).map(d(f)));
    e[f] = {timestamp:g.mtime};
  }
  return b(null, {type:"local", entries:e});
}, lc:(a, b) => {
  var c = {};
  I.ic(a.ua, (d, e) => {
    if (d) {
      return b(d);
    }
    try {
      var f = e.transaction([I.ea], "readonly");
      f.onerror = g => {
        b(g.target.error);
        g.preventDefault();
      };
      f.objectStore(I.ea).index("timestamp").openKeyCursor().onsuccess = g => {
        g = g.target.result;
        if (!g) {
          return b(null, {type:"remote", db:e, entries:c});
        }
        c[g.primaryKey] = {timestamp:g.key};
        g.continue();
      };
    } catch (g) {
      return b(g);
    }
  });
}, xc:(a, b) => {
  try {
    var c = J(a).node;
    var d = E.lstat(a);
  } catch (e) {
    return b(e);
  }
  return G(d.mode) ? b(null, {timestamp:d.mtime, mode:d.mode}) : 40960 === (d.mode & 61440) ? b(null, {timestamp:d.mtime, mode:d.mode, link:c.link}) : E.isFile(d.mode) ? (c.l = Eb(c), b(null, {timestamp:d.mtime, mode:d.mode, contents:c.l})) : b(Error("node type not supported"));
}, Kc:(a, b, c) => {
  try {
    if (G(b.mode)) {
      var d = b.mode, e = a.split("/"), f = "", g;
      for (g of e) {
        if (g) {
          if (f || "/" === a.charAt(0)) {
            f += "/";
          }
          f += g;
          try {
            E.mkdir(f, d);
          } catch (t) {
            if (20 != t.s) {
              throw t;
            }
          }
        }
      }
    } else if (40960 === (b.mode & 61440)) {
      E.symlink(b.link, a);
    } else if (E.isFile(b.mode)) {
      E.writeFile(a, b.contents, {Sb:!0});
    } else {
      return c(Error("node type not supported"));
    }
    E.chmod(a, b.mode);
    var l = b.timestamp, n = b.timestamp, r = J(a, {K:!0}).node;
    Jb(r.h.R, 63)(r, {atime:l, mtime:n});
  } catch (t) {
    return c(t);
  }
  c(null);
}, Hc:(a, b) => {
  try {
    var c = E.lstat(a);
    G(c.mode) ? E.rmdir(a) : E.unlink(a);
  } catch (d) {
    return b(d);
  }
  b(null);
}, yc:(a, b, c) => {
  a = a.get(b);
  a.onsuccess = d => c(null, d.target.result);
  a.onerror = d => {
    c(d.target.error);
    d.preventDefault();
  };
}, Lc:(a, b, c, d) => {
  try {
    var e = a.put(c, b);
  } catch (f) {
    d(f);
    return;
  }
  e.onsuccess = () => d();
  e.onerror = f => {
    d(f.target.error);
    f.preventDefault();
  };
}, Ic:(a, b, c) => {
  a = a.delete(b);
  a.onsuccess = () => c();
  a.onerror = d => {
    c(d.target.error);
    d.preventDefault();
  };
}, Fc:(a, b, c) => {
  function d(u) {
    if (u && !r) {
      return r = !0, c(u);
    }
  }
  var e = 0, f = [], g;
  for ([g, n] of Object.entries(a.entries)) {
    var l = b.entries[g];
    l && n.timestamp.getTime() == l.timestamp.getTime() || (f.push(g), e++);
  }
  var n = [];
  for (g of Object.keys(b.entries)) {
    a.entries[g] || (n.push(g), e++);
  }
  if (!e) {
    return c(null);
  }
  var r = !1;
  a = ("remote" === a.type ? a.db : b.db).transaction([I.ea], "readwrite");
  var t = a.objectStore(I.ea);
  a.onerror = a.onabort = u => {
    d(u.target.error);
    u.preventDefault();
  };
  a.oncomplete = () => {
    r || c(null);
  };
  for (const u of f.sort()) {
    "local" === b.type ? I.yc(t, u, (H, N) => {
      if (H) {
        return d(H);
      }
      I.Kc(u, N, d);
    }) : I.xc(u, (H, N) => {
      if (H) {
        return d(H);
      }
      I.Lc(t, u, N, d);
    });
  }
  for (var v of n.sort().reverse()) {
    "local" === b.type ? I.Hc(v, d) : I.Ic(t, v, d);
  }
}}, Kb = {EPERM:63, ENOENT:44, ESRCH:71, EINTR:27, EIO:29, ENXIO:60, E2BIG:1, ENOEXEC:45, EBADF:8, ECHILD:12, EAGAIN:6, EWOULDBLOCK:6, ENOMEM:48, EACCES:2, EFAULT:21, ENOTBLK:105, EBUSY:10, EEXIST:20, EXDEV:75, ENODEV:43, ENOTDIR:54, EISDIR:31, EINVAL:28, ENFILE:41, EMFILE:33, ENOTTY:59, ETXTBSY:74, EFBIG:22, ENOSPC:51, ESPIPE:70, EROFS:69, EMLINK:34, EPIPE:64, EDOM:18, ERANGE:68, ENOMSG:49, EIDRM:24, ECHRNG:106, EL2NSYNC:156, EL3HLT:107, EL3RST:108, ELNRNG:109, EUNATCH:110, ENOCSI:111, EL2HLT:112, 
EDEADLK:16, ENOLCK:46, EBADE:113, EBADR:114, EXFULL:115, ENOANO:104, EBADRQC:103, EBADSLT:102, EDEADLOCK:16, EBFONT:101, ENOSTR:100, ENODATA:116, ETIME:117, ENOSR:118, ENONET:119, ENOPKG:120, EREMOTE:121, ENOLINK:47, EADV:122, ESRMNT:123, ECOMM:124, EPROTO:65, EMULTIHOP:36, EDOTDOT:125, EBADMSG:9, ENOTUNIQ:126, EBADFD:127, EREMCHG:128, ELIBACC:129, ELIBBAD:130, ELIBSCN:131, ELIBMAX:132, ELIBEXEC:133, ENOSYS:52, ENOTEMPTY:55, ENAMETOOLONG:37, ELOOP:32, EOPNOTSUPP:138, EPFNOSUPPORT:139, ECONNRESET:15, 
ENOBUFS:42, EAFNOSUPPORT:5, EPROTOTYPE:67, ENOTSOCK:57, ENOPROTOOPT:50, ESHUTDOWN:140, ECONNREFUSED:14, EADDRINUSE:3, ECONNABORTED:13, ENETUNREACH:40, ENETDOWN:38, ETIMEDOUT:73, EHOSTDOWN:142, EHOSTUNREACH:23, EINPROGRESS:26, EALREADY:7, EDESTADDRREQ:17, EMSGSIZE:35, EPROTONOSUPPORT:66, ESOCKTNOSUPPORT:137, EADDRNOTAVAIL:4, ENETRESET:39, EISCONN:30, ENOTCONN:53, ETOOMANYREFS:141, EUSERS:136, EDQUOT:19, ESTALE:72, ENOTSUP:138, ENOMEDIUM:148, EILSEQ:25, EOVERFLOW:61, ECANCELED:11, ENOTRECOVERABLE:56, 
EOWNERDEAD:62, ESTRPIPE:135}, Lb = async a => {
  var b = await na(a);
  k(b, `Loading data file "${a}" failed (no arrayBuffer).`);
  return new Uint8Array(b);
}, Mb = [], Pb = async(a, b) => {
  "undefined" != typeof Nb && Ob();
  for (var c of Mb) {
    if (c.canHandle(b)) {
      return k("AsyncFunction" === c.handle.constructor.name, "Filesystem plugin handlers must be async functions (See #24914)"), c.handle(a, b);
    }
  }
  return a;
}, Rb = async(a, b, c, d, e, f, g, l) => {
  var n = b ? rb(lb(a + "/" + b)) : a, r;
  a: {
    for (var t = r = `cp ${n}`;;) {
      if (!$a[r]) {
        break a;
      }
      r = t + Math.random();
    }
  }
  cb(r);
  try {
    t = c, "string" == typeof c && (t = await Lb(c)), t = await Pb(t, n), l?.(), f || Qb(a, b, t, d, e, g);
  } finally {
    bb(r);
  }
};
function zb(a, b) {
  E.xb[a] = {j:b};
}
function G(a) {
  return 16384 === (a & 61440);
}
function Fb(a, b) {
  var c = G(a.mode) ? (c = Sb(a, "x")) ? c : a.h.lookup ? 0 : 2 : 54;
  if (c) {
    throw new E.g(c);
  }
  for (c = E.Y[Tb(a.id, b)]; c; c = c.ha) {
    var d = c.name;
    if (c.parent.id === a.id && d === b) {
      return c;
    }
  }
  return E.lookup(a, b);
}
function Gb(a) {
  var b = Tb(a.parent.id, a.name);
  if (E.Y[b] === a) {
    E.Y[b] = a.ha;
  } else {
    for (b = E.Y[b]; b;) {
      if (b.ha === a) {
        b.ha = a.ha;
        break;
      }
      b = b.ha;
    }
  }
}
function J(a, b = {}) {
  if (!a) {
    throw new E.g(44);
  }
  b.Fa ?? (b.Fa = !0);
  "/" === a.charAt(0) || (a = E.cwd() + "/" + a);
  var c = 0;
  a: for (; 40 > c; c++) {
    a = a.split("/").filter(l => !!l);
    for (var d = E.root, e = "/", f = 0; f < a.length; f++) {
      var g = f === a.length - 1;
      if (g && b.parent) {
        break;
      }
      if ("." !== a[f]) {
        if (".." === a[f]) {
          if (e = mb(e), E.ta(d)) {
            a = e + "/" + a.slice(f + 1).join("/");
            c--;
            continue a;
          } else {
            d = d.parent;
          }
        } else {
          e = lb(e + "/" + a[f]);
          try {
            d = Fb(d, a[f]);
          } catch (l) {
            if (44 === l?.s && g && b.zc) {
              return {path:e};
            }
            throw l;
          }
          !d.O || g && !b.Fa || (d = d.O.root);
          if (40960 === (d.mode & 61440) && (!g || b.K)) {
            if (!d.h.readlink) {
              throw new E.g(52);
            }
            d = d.h.readlink(d);
            "/" === d.charAt(0) || (d = mb(e) + "/" + d);
            a = d + "/" + a.slice(f + 1).join("/");
            continue a;
          }
        }
      }
    }
    return {path:e, node:d};
  }
  throw new E.g(32);
}
function Qb(a, b, c, d, e, f) {
  var g = b;
  a && (a = "string" == typeof a ? a : Ub(a), g = b ? lb(a + "/" + b) : a);
  a = Ib(d, e);
  g = E.create(g, a);
  c && (c = Hb(c), E.chmod(g, a | 146), b = E.open(g, 577), E.write(b, c, 0, c.length, 0, f), E.close(b), E.chmod(g, a));
}
function Ub(a) {
  for (var b;;) {
    if (E.ta(a)) {
      return a = a.m.ua, b ? "/" !== a[a.length - 1] ? `${a}/${b}` : a + b : a;
    }
    b = b ? `${a.name}/${b}` : a.name;
    a = a.parent;
  }
}
function Tb(a, b) {
  for (var c = 0, d = 0; d < b.length; d++) {
    c = (c << 5) - c + b.charCodeAt(d) | 0;
  }
  return (a + c >>> 0) % E.Y.length;
}
function Vb(a) {
  var b = Tb(a.parent.id, a.name);
  a.ha = E.Y[b];
  E.Y[b] = a;
}
function Sb(a, b) {
  return E.Bb ? 0 : b.includes("r") && !(a.mode & 292) || b.includes("w") && !(a.mode & 146) || b.includes("x") && !(a.mode & 73) ? 2 : 0;
}
function Wb(a, b) {
  if (!G(a.mode)) {
    return 54;
  }
  try {
    return Fb(a, b), 20;
  } catch (c) {
  }
  return Sb(a, "wx");
}
function Xb(a, b, c) {
  try {
    var d = Fb(a, b);
  } catch (e) {
    return e.s;
  }
  if (a = Sb(a, "wx")) {
    return a;
  }
  if (c) {
    if (!G(d.mode)) {
      return 54;
    }
    if (E.ta(d) || Ub(d) === E.cwd()) {
      return 10;
    }
  } else if (G(d.mode)) {
    return 31;
  }
  return 0;
}
function Jb(a, b) {
  if (!a) {
    throw new E.g(b);
  }
  return a;
}
function Yb(a) {
  a = E.mc(a);
  if (!a) {
    throw new E.g(8);
  }
  return a;
}
function Zb(a, b = -1) {
  k(-1 <= b);
  a = Object.assign(new E.Pb(), a);
  if (-1 == b) {
    a: {
      for (b = 0; b <= E.Qb; b++) {
        if (!E.streams[b]) {
          break a;
        }
      }
      throw new E.g(33);
    }
  }
  a.fd = b;
  return E.streams[b] = a;
}
function $b(a, b = -1) {
  a = Zb(a, b);
  a.j?.hd?.(a);
  return a;
}
function ac(a, b, c) {
  var d = a?.j.R;
  a = d ? a : b;
  d ??= b.h.R;
  Jb(d, 63);
  d(a, c);
}
function bc(a) {
  var b = [];
  for (a = [a]; a.length;) {
    var c = a.pop();
    b.push(c);
    a.push(...c.va);
  }
  return b;
}
function cc(a) {
  var b = {Uc:4096, nd:4096, blocks:1e6, Tc:5e5, Sc:5e5, files:E.fb, kd:E.fb - 1, od:42, flags:2, wd:255};
  a.h.Kb && Object.assign(b, a.h.Kb(a.m.Hb.root));
  return b;
}
function dc(a, b, c) {
  "undefined" == typeof c && (c = b, b = 438);
  return E.N(a, b | 8192, c);
}
function ec(a, b, c, d) {
  ac(a, b, {mode:c & 4095 | b.mode & -4096, ctime:Date.now(), yb:d});
}
function fc(a, b, c) {
  if (G(b.mode)) {
    throw new E.g(31);
  }
  if (!E.isFile(b.mode)) {
    throw new E.g(28);
  }
  var d = Sb(b, "w");
  if (d) {
    throw new E.g(d);
  }
  ac(a, b, {size:c, timestamp:Date.now()});
}
function gc(a, b) {
  try {
    var c = J(a, {K:!b});
    a = c.path;
  } catch (e) {
  }
  var d = {ta:!1, exists:!1, error:0, name:null, path:null, object:null, Ac:!1, Cc:null, Bc:null};
  try {
    c = J(a, {parent:!0}), d.Ac = !0, d.Cc = c.path, d.Bc = c.node, d.name = nb(a), c = J(a, {K:!b}), d.exists = !0, d.path = c.path, d.object = c.node, d.name = c.node.name, d.ta = "/" === c.path;
  } catch (e) {
    d.error = e.s;
  }
  return d;
}
function hc(a, b) {
  a = "string" == typeof a ? a : Ub(a);
  for (b = b.split("/").reverse(); b.length;) {
    var c = b.pop();
    if (c) {
      var d = lb(a + "/" + c);
      try {
        E.mkdir(d);
      } catch (e) {
        if (20 != e.s) {
          throw e;
        }
      }
      a = d;
    }
  }
  return d;
}
function ic(a, b, c, d) {
  a = "string" == typeof a ? a : Ub(a);
  b = lb(a + "/" + b);
  return E.create(b, Ib(c, d));
}
function jc(a) {
  if (!(a.vc || a.wc || a.link || a.l)) {
    if (globalThis.XMLHttpRequest) {
      p("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
    } else {
      try {
        a.l = oa(a.url);
      } catch (b) {
        throw new E.g(29);
      }
    }
  }
}
function kc(a, b, c, d, e) {
  function f(t, v, u, H, N) {
    t = t.node.l;
    if (N >= t.length) {
      return 0;
    }
    H = Math.min(t.length - N, H);
    k(0 <= H);
    if (t.slice) {
      for (var S = 0; S < H; S++) {
        v[u + S] = t[N + S];
      }
    } else {
      for (S = 0; S < H; S++) {
        v[u + S] = t.get(N + S);
      }
    }
    return H;
  }
  class g {
    Za=!1;
    G=[];
    Xa=void 0;
    Fb=0;
    Eb=0;
    get(t) {
      if (!(t > this.length - 1 || 0 > t)) {
        var v = t % this.chunkSize;
        return this.Xa(t / this.chunkSize | 0)[v];
      }
    }
    Mc(t) {
      this.Xa = t;
    }
    Jb() {
      var t = new XMLHttpRequest();
      t.open("HEAD", c, !1);
      t.send(null);
      200 <= t.status && 300 > t.status || 304 === t.status || p("Couldn't load " + c + ". Status: " + t.status);
      var v = Number(t.getResponseHeader("Content-length")), u, H = (u = t.getResponseHeader("Accept-Ranges")) && "bytes" === u;
      t = (u = t.getResponseHeader("Content-Encoding")) && "gzip" === u;
      var N = 1048576;
      H || (N = v);
      var S = this;
      S.Mc(ba => {
        var U = ba * N, ea = (ba + 1) * N - 1;
        ea = Math.min(ea, v - 1);
        if ("undefined" == typeof S.G[ba]) {
          var zc = S.G;
          U > ea && p("invalid range (" + U + ", " + ea + ") or no bytes requested!");
          ea > v - 1 && p("only " + v + " bytes available! programmer error!");
          var W = new XMLHttpRequest();
          W.open("GET", c, !1);
          v !== N && W.setRequestHeader("Range", "bytes=" + U + "-" + ea);
          W.responseType = "arraybuffer";
          W.overrideMimeType && W.overrideMimeType("text/plain; charset=x-user-defined");
          W.send(null);
          200 <= W.status && 300 > W.status || 304 === W.status || p("Couldn't load " + c + ". Status: " + W.status);
          U = void 0 !== W.response ? new Uint8Array(W.response || []) : wb(W.responseText || "");
          zc[ba] = U;
        }
        "undefined" == typeof S.G[ba] && p("doXHR failed!");
        return S.G[ba];
      });
      if (t || !v) {
        N = v = 1, N = v = this.Xa(0).length, qa("LazyFiles on gzip forces download of the whole file when length is accessed");
      }
      this.Fb = v;
      this.Eb = N;
      this.Za = !0;
    }
    get length() {
      this.Za || this.Jb();
      return this.Fb;
    }
    get chunkSize() {
      this.Za || this.Jb();
      return this.Eb;
    }
  }
  if (globalThis.XMLHttpRequest) {
    ca || p("Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc");
    var l = new g();
    var n = void 0;
  } else {
    n = c, l = void 0;
  }
  var r = ic(a, b, d, e);
  l ? r.l = l : n && (r.l = null, r.url = n);
  Object.defineProperties(r, {A:{get:function() {
    return this.l.length;
  }}});
  a = {};
  for (const [t, v] of Object.entries(r.j)) {
    a[t] = (...u) => {
      jc(r);
      return v(...u);
    };
  }
  a.read = (t, v, u, H, N) => {
    jc(r);
    return f(t, v, u, H, N);
  };
  a.na = (t, v, u) => {
    jc(r);
    var H = Db();
    if (!H) {
      throw new E.g(48);
    }
    f(t, x, H, v, u);
    return {oa:H, Rb:!0};
  };
  r.j = a;
  return r;
}
var E = {root:null, va:[], xb:{}, streams:[], fb:1, Y:null, wb:"/", Ga:!1, Bb:!0, fc:null, xa:0, g:class extends Error {
  name="ErrnoError";
  constructor(a) {
    super(Ha ? D(lc(a)) : "");
    this.s = a;
    for (var b in Kb) {
      if (Kb[b] === a) {
        this.code = b;
        break;
      }
    }
  }
}, Pb:class {
  G={};
  node=null;
  get object() {
    return this.node;
  }
  set object(a) {
    this.node = a;
  }
  get flags() {
    return this.G.flags;
  }
  set flags(a) {
    this.G.flags = a;
  }
  get position() {
    return this.G.position;
  }
  set position(a) {
    this.G.position = a;
  }
}, Ob:class {
  h={};
  j={};
  O=null;
  constructor(a, b, c, d) {
    a ||= this;
    this.parent = a;
    this.m = a.m;
    this.id = E.fb++;
    this.name = b;
    this.mode = c;
    this.rdev = d;
    this.atime = this.mtime = this.ctime = Date.now();
  }
  get read() {
    return 365 === (this.mode & 365);
  }
  set read(a) {
    a ? this.mode |= 365 : this.mode &= -366;
  }
  get write() {
    return 146 === (this.mode & 146);
  }
  set write(a) {
    a ? this.mode |= 146 : this.mode &= -147;
  }
  get wc() {
    return G(this.mode);
  }
  get vc() {
    return 8192 === (this.mode & 61440);
  }
}, createNode(a, b, c, d) {
  k("object" == typeof a);
  a = new E.Ob(a, b, c, d);
  Vb(a);
  return a;
}, ta(a) {
  return a === a.parent;
}, isFile(a) {
  return 32768 === (a & 61440);
}, isFIFO(a) {
  return 4096 === (a & 61440);
}, isSocket(a) {
  return 49152 === (a & 49152);
}, Qb:4096, mc:a => E.streams[a], Tb:{open(a) {
  a.j = E.jc(a.node.rdev).j;
  a.j.open?.(a);
}, D() {
  throw new E.g(70);
}}, $a:a => a >> 8, td:a => a & 255, ga:(a, b) => a << 8 | b, jc:a => E.xb[a], La(a, b) {
  function c(l) {
    k(0 < E.xa);
    E.xa--;
    return b(l);
  }
  function d(l) {
    if (l) {
      if (!d.cc) {
        return d.cc = !0, c(l);
      }
    } else {
      ++f >= e.length && c(null);
    }
  }
  "function" == typeof a && (b = a, a = !1);
  E.xa++;
  1 < E.xa && m(`warning: ${E.xa} FS.syncfs operations in flight at once, probably just doing extra work`);
  var e = bc(E.root.m), f = 0, g;
  for (g of e) {
    g.type.La ? g.type.La(g, a, d) : d(null);
  }
}, m(a, b, c) {
  if ("string" == typeof a) {
    throw a;
  }
  var d = "/" === c, e = !c;
  if (d && E.root) {
    throw new E.g(10);
  }
  if (!d && !e) {
    var f = J(c, {Fa:!1});
    c = f.path;
    f = f.node;
    if (f.O) {
      throw new E.g(10);
    }
    if (!G(f.mode)) {
      throw new E.g(54);
    }
  }
  b = {type:a, Hb:b, ua:c, va:[]};
  a = a.m(b);
  a.m = b;
  b.root = a;
  d ? E.root = a : f && (f.O = b, f.m && f.m.va.push(b));
  return a;
}, Dd(a) {
  a = J(a, {Fa:!1});
  if (!a.node.O) {
    throw new E.g(28);
  }
  a = a.node;
  var b = a.O, c = bc(b);
  for ([, e] of Object.entries(E.Y)) {
    for (; e;) {
      var d = e.ha;
      c.includes(e.m) && Gb(e);
      var e = d;
    }
  }
  a.O = null;
  e = a.m.va.indexOf(b);
  k(-1 !== e);
  a.m.va.splice(e, 1);
}, lookup(a, b) {
  return a.h.lookup(a, b);
}, N(a, b, c) {
  var d = J(a, {parent:!0}).node;
  a = nb(a);
  if (!a) {
    throw new E.g(28);
  }
  if ("." === a || ".." === a) {
    throw new E.g(20);
  }
  var e = Wb(d, a);
  if (e) {
    throw new E.g(e);
  }
  if (!d.h.N) {
    throw new E.g(63);
  }
  return d.h.N(d, a, b, c);
}, Kb(a) {
  return cc(J(a, {K:!0}).node);
}, Bd(a) {
  return cc(a.node);
}, create(a, b = 438) {
  return E.N(a, b & 4095 | 32768, 0);
}, mkdir(a, b = 511) {
  return E.N(a, b & 1023 | 16384, 0);
}, symlink(a, b) {
  if (!rb(a)) {
    throw new E.g(44);
  }
  var c = J(b, {parent:!0}).node;
  if (!c) {
    throw new E.g(44);
  }
  b = nb(b);
  var d = Wb(c, b);
  if (d) {
    throw new E.g(d);
  }
  if (!c.h.symlink) {
    throw new E.g(63);
  }
  return c.h.symlink(c, b, a);
}, rename(a, b) {
  var c = mb(a), d = mb(b), e = nb(a), f = nb(b);
  var g = J(a, {parent:!0});
  var l = g.node;
  g = J(b, {parent:!0});
  g = g.node;
  if (!l || !g) {
    throw new E.g(44);
  }
  if (l.m !== g.m) {
    throw new E.g(75);
  }
  var n = Fb(l, e);
  a = sb(a, d);
  if ("." !== a.charAt(0)) {
    throw new E.g(28);
  }
  a = sb(b, c);
  if ("." !== a.charAt(0)) {
    throw new E.g(55);
  }
  try {
    var r = Fb(g, f);
  } catch (t) {
  }
  if (n !== r) {
    b = G(n.mode);
    if (e = Xb(l, e, b)) {
      throw new E.g(e);
    }
    if (e = r ? Xb(g, f, b) : Wb(g, f)) {
      throw new E.g(e);
    }
    if (!l.h.rename) {
      throw new E.g(63);
    }
    if (n.O || r && r.O) {
      throw new E.g(10);
    }
    if (g !== l && (e = Sb(l, "w"))) {
      throw new E.g(e);
    }
    Gb(n);
    try {
      l.h.rename(n, g, f), n.parent = g;
    } catch (t) {
      throw t;
    } finally {
      Vb(n);
    }
  }
}, rmdir(a) {
  var b = J(a, {parent:!0}).node;
  a = nb(a);
  var c = Fb(b, a), d = Xb(b, a, !0);
  if (d) {
    throw new E.g(d);
  }
  if (!b.h.rmdir) {
    throw new E.g(63);
  }
  if (c.O) {
    throw new E.g(10);
  }
  b.h.rmdir(b, a);
  Gb(c);
}, readdir(a) {
  a = J(a, {K:!0}).node;
  return Jb(a.h.readdir, 54)(a);
}, unlink(a) {
  var b = J(a, {parent:!0}).node;
  if (!b) {
    throw new E.g(44);
  }
  a = nb(a);
  var c = Fb(b, a), d = Xb(b, a, !1);
  if (d) {
    throw new E.g(d);
  }
  if (!b.h.unlink) {
    throw new E.g(63);
  }
  if (c.O) {
    throw new E.g(10);
  }
  b.h.unlink(b, a);
  Gb(c);
}, readlink(a) {
  a = J(a).node;
  if (!a) {
    throw new E.g(44);
  }
  if (!a.h.readlink) {
    throw new E.g(28);
  }
  return a.h.readlink(a);
}, stat(a, b) {
  a = J(a, {K:!b}).node;
  return Jb(a.h.L, 63)(a);
}, fstat(a) {
  var b = Yb(a);
  a = b.node;
  var c = b.j.L;
  b = c ? b : a;
  c ??= a.h.L;
  Jb(c, 63);
  return c(b);
}, lstat(a) {
  return E.stat(a, !0);
}, chmod(a, b, c) {
  a = "string" == typeof a ? J(a, {K:!c}).node : a;
  ec(null, a, b, c);
}, lchmod(a, b) {
  E.chmod(a, b, !0);
}, fchmod(a, b) {
  a = Yb(a);
  ec(a, a.node, b, !1);
}, chown(a, b, c, d) {
  a = "string" == typeof a ? J(a, {K:!d}).node : a;
  ac(null, a, {timestamp:Date.now(), yb:d});
}, lchown(a, b, c) {
  E.chown(a, b, c, !0);
}, fchown(a) {
  a = Yb(a);
  ac(a, a.node, {timestamp:Date.now(), yb:!1});
}, truncate(a, b) {
  if (0 > b) {
    throw new E.g(28);
  }
  a = "string" == typeof a ? J(a, {K:!0}).node : a;
  fc(null, a, b);
}, pd(a, b) {
  a = Yb(a);
  if (0 > b || 0 === (a.flags & 2097155)) {
    throw new E.g(28);
  }
  fc(a, a.node, b);
}, open(a, b, c = 438) {
  if ("" === a) {
    throw new E.g(44);
  }
  if ("string" == typeof b) {
    var d = {r:0, "r+":2, w:577, "w+":578, a:1089, "a+":1090}[b];
    if ("undefined" == typeof d) {
      throw Error(`Unknown file open mode: ${b}`);
    }
    b = d;
  }
  c = b & 64 ? c & 4095 | 32768 : 0;
  if ("object" == typeof a) {
    d = a;
  } else {
    var e = a.endsWith("/");
    var f = J(a, {K:!(b & 131072), zc:!0});
    d = f.node;
    a = f.path;
  }
  f = !1;
  if (b & 64) {
    if (d) {
      if (b & 128) {
        throw new E.g(20);
      }
    } else {
      if (e) {
        throw new E.g(31);
      }
      d = E.N(a, c | 511, 0);
      f = !0;
    }
  }
  if (!d) {
    throw new E.g(44);
  }
  8192 === (d.mode & 61440) && (b &= -513);
  if (b & 65536 && !G(d.mode)) {
    throw new E.g(54);
  }
  if (!f && (d ? 40960 === (d.mode & 61440) ? e = 32 : (e = ["r", "w", "rw"][b & 3], b & 512 && (e += "w"), e = G(d.mode) && ("r" !== e || b & 576) ? 31 : Sb(d, e)) : e = 44, e)) {
    throw new E.g(e);
  }
  b & 512 && !f && E.truncate(d, 0);
  b &= -131713;
  b = Zb({node:d, path:Ub(d), flags:b, seekable:!0, position:0, j:d.j, Nc:[], error:!1});
  b.j.open && b.j.open(b);
  f && E.chmod(d, c & 511);
  return b;
}, close(a) {
  if (null === a.fd) {
    throw new E.g(8);
  }
  a.Ua && (a.Ua = null);
  try {
    a.j.close && a.j.close(a);
  } catch (b) {
    throw b;
  } finally {
    E.streams[a.fd] = null;
  }
  a.fd = null;
}, D(a, b, c) {
  if (null === a.fd) {
    throw new E.g(8);
  }
  if (!a.seekable || !a.j.D) {
    throw new E.g(70);
  }
  if (0 != c && 1 != c && 2 != c) {
    throw new E.g(28);
  }
  a.position = a.j.D(a, b, c);
  a.Nc = [];
  return a.position;
}, read(a, b, c, d, e) {
  k(0 <= c);
  if (0 > d || 0 > e) {
    throw new E.g(28);
  }
  if (null === a.fd) {
    throw new E.g(8);
  }
  if (1 === (a.flags & 2097155)) {
    throw new E.g(8);
  }
  if (G(a.node.mode)) {
    throw new E.g(31);
  }
  if (!a.j.read) {
    throw new E.g(28);
  }
  var f = "undefined" != typeof e;
  if (!f) {
    e = a.position;
  } else if (!a.seekable) {
    throw new E.g(70);
  }
  b = a.j.read(a, b, c, d, e);
  f || (a.position += b);
  return b;
}, write(a, b, c, d, e, f) {
  k(0 <= c);
  k(b.subarray, "FS.write expects a TypedArray");
  if (0 > d || 0 > e) {
    throw new E.g(28);
  }
  if (null === a.fd) {
    throw new E.g(8);
  }
  if (0 === (a.flags & 2097155)) {
    throw new E.g(8);
  }
  if (G(a.node.mode)) {
    throw new E.g(31);
  }
  if (!a.j.write) {
    throw new E.g(28);
  }
  a.seekable && a.flags & 1024 && E.D(a, 0, 2);
  var g = "undefined" != typeof e;
  if (!g) {
    e = a.position;
  } else if (!a.seekable) {
    throw new E.g(70);
  }
  b = a.j.write(a, b, c, d, e, f);
  g || (a.position += b);
  return b;
}, na(a, b, c, d, e) {
  if (0 !== (d & 2) && 0 === (e & 2) && 2 !== (a.flags & 2097155)) {
    throw new E.g(2);
  }
  if (1 === (a.flags & 2097155)) {
    throw new E.g(2);
  }
  if (!a.j.na) {
    throw new E.g(43);
  }
  if (!b) {
    throw new E.g(28);
  }
  return a.j.na(a, b, c, d, e);
}, wa(a, b, c, d, e) {
  k(0 <= c);
  return a.j.wa ? a.j.wa(a, b, c, d, e) : 0;
}, Wa(a, b, c) {
  if (!a.j.Wa) {
    throw new E.g(59);
  }
  return a.j.Wa(a, b, c);
}, readFile(a, b = {}) {
  b.flags = b.flags || 0;
  b.encoding = b.encoding || "binary";
  "utf8" !== b.encoding && "binary" !== b.encoding && p(`Invalid encoding type "${b.encoding}"`);
  var c = E.open(a, b.flags);
  a = E.stat(a).size;
  var d = new Uint8Array(a);
  E.read(c, d, 0, a, 0);
  "utf8" === b.encoding && (d = hb(d));
  E.close(c);
  return d;
}, writeFile(a, b, c = {}) {
  c.flags = c.flags || 577;
  a = E.open(a, c.flags, c.mode);
  b = Hb(b);
  E.write(a, b, 0, b.byteLength, void 0, c.Sb);
  E.close(a);
}, cwd:() => E.wb, chdir(a) {
  a = J(a, {K:!0});
  if (null === a.node) {
    throw new E.g(44);
  }
  if (!G(a.node.mode)) {
    throw new E.g(54);
  }
  var b = Sb(a.node, "x");
  if (b) {
    throw new E.g(b);
  }
  E.wb = a.path;
}, Ec() {
  E.Ga = !1;
  mc(0);
  for (var a of E.streams) {
    a && E.close(a);
  }
}, ld(a, b) {
  a = gc(a, b);
  return a.exists ? a.object : null;
}, $(a, b, c, d) {
  a = ob("string" == typeof a ? a : Ub(a), b);
  b = Ib(!!c, !!d);
  var e;
  (e = E.$).$a ?? (e.$a = 64);
  e = E.ga(E.$.$a++, 0);
  zb(e, {open(f) {
    f.seekable = !1;
  }, close() {
    d?.buffer?.length && d(10);
  }, read(f, g, l, n) {
    for (var r = 0, t = 0; t < n; t++) {
      try {
        var v = c();
      } catch (u) {
        throw new E.g(29);
      }
      if (void 0 === v && 0 === r) {
        throw new E.g(6);
      }
      if (null === v || void 0 === v) {
        break;
      }
      r++;
      g[l + t] = v;
    }
    r && (f.node.atime = Date.now());
    return r;
  }, write(f, g, l, n) {
    for (var r = 0; r < n; r++) {
      try {
        d(g[l + r]);
      } catch (t) {
        throw new E.g(29);
      }
    }
    n && (f.node.mtime = f.node.ctime = Date.now());
    return r;
  }});
  return dc(a, b, e);
}};
function nc(a, b, c) {
  if ("/" === b.charAt(0)) {
    return b;
  }
  a = -100 === a ? E.cwd() : Yb(a).path;
  if (0 == b.length) {
    if (!c) {
      throw new E.g(44);
    }
    return a;
  }
  return a + "/" + b;
}
function oc(a, b) {
  q[a >> 2] = b.dev;
  q[a + 4 >> 2] = b.mode;
  q[a + 8 >> 2] = b.nlink;
  q[a + 12 >> 2] = b.uid;
  q[a + 16 >> 2] = b.gid;
  q[a + 20 >> 2] = b.rdev;
  Na[a + 24 >> 3] = BigInt(b.size);
  z[a + 32 >> 2] = 4096;
  z[a + 36 >> 2] = b.blocks;
  var c = b.atime.getTime(), d = b.mtime.getTime(), e = b.ctime.getTime();
  Na[a + 40 >> 3] = BigInt(Math.floor(c / 1000));
  q[a + 48 >> 2] = c % 1000 * 1E6;
  Na[a + 56 >> 3] = BigInt(Math.floor(d / 1000));
  q[a + 64 >> 2] = d % 1000 * 1E6;
  Na[a + 72 >> 3] = BigInt(Math.floor(e / 1000));
  q[a + 80 >> 2] = e % 1000 * 1E6;
  Na[a + 88 >> 3] = BigInt(b.ino);
  return 0;
}
var ib = void 0, pc = a => 0 === a % 4 && (0 !== a % 100 || 0 === a % 400), qc = [0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335], rc = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334], K = (a, b, c) => {
  k("number" == typeof c, "stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!");
  return vb(a, y, b, c);
}, tc = a => {
  a instanceof Ta || "unwind" == a || (wa(), a instanceof WebAssembly.RuntimeError && 0 >= sc() && m("Stack overflow detected.  You can try increasing -sSTACK_SIZE (currently set to 65536)"), ka(1, a));
}, uc = 0, wc = (a, b) => {
  ta = a;
  vc();
  (eb || 0 < uc) && !b && m(`program exited (with status: ${a}), but keepRuntimeAlive() is set (counter=${uc}) due to an async operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to force a true shutdown)`);
  ta = a;
  eb || 0 < uc || (h.onExit?.(a), sa = !0);
  ka(a, new Ta(a));
}, xc = () => {
  if (!(eb || 0 < uc)) {
    try {
      wc(ta);
    } catch (a) {
      tc(a);
    }
  }
}, yc = a => {
  if (sa) {
    m("user callback triggered after runtime exited or application aborted.  Ignoring.");
  } else {
    try {
      return a();
    } catch (b) {
      tc(b);
    } finally {
      xc();
    }
  }
};
function Ac() {
  return document.fullscreenElement || document.mozFullScreenElement || document.webkitFullscreenElement || document.webkitCurrentFullScreenElement || document.msFullscreenElement;
}
var Bc = a => {
  setTimeout(() => {
    yc(a);
  }, 10000);
}, Cc = !1, Dc = [];
function Ob() {
  function a() {
    Cc = document.pointerLockElement === h.canvas;
  }
  if (!Ec) {
    Ec = !0;
    Mb.push({canHandle:c => !h.noImageDecoding && /\.(jpg|jpeg|png|bmp|webp)$/i.test(c), handle:async(c, d) => {
      var e = new Blob([c], {type:Fc(d)});
      e.size !== c.length && (e = new Blob([(new Uint8Array(c)).buffer], {type:Fc(d)}));
      var f = URL.createObjectURL(e);
      return new Promise((g, l) => {
        var n = new Image();
        n.onload = () => {
          k(n.complete, `Image ${d} could not be decoded`);
          var r = document.createElement("canvas");
          r.width = n.width;
          r.height = n.height;
          r.getContext("2d").drawImage(n, 0, 0);
          URL.revokeObjectURL(f);
          g(c);
        };
        n.onerror = () => {
          m(`Image ${f} could not be decoded`);
          l();
        };
        n.src = f;
      });
    }});
    Mb.push({canHandle:c => !h.noAudioDecoding && c.slice(-4) in {".ogg":1, ".wav":1, ".mp3":1}, handle:async(c, d) => new Promise(e => {
      function f() {
        g || (g = !0, e(c));
      }
      var g = !1, l = new Blob([c], {type:Fc(d)});
      l = URL.createObjectURL(l);
      var n = new Audio();
      n.addEventListener("canplaythrough", () => f(n), !1);
      n.onerror = () => {
        if (!g) {
          m(`warning: browser could not fully decode audio ${d}, trying slower base64 approach`);
          for (var r = "data:audio/x-" + d.slice(-3) + ";base64,", t = "", v = 0, u = 0, H = 0; H < c.length; H++) {
            for (v = v << 8 | c[H], u += 8; 6 <= u;) {
              var N = v >> u - 6 & 63;
              u -= 6;
              t += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[N];
            }
          }
          2 == u ? (t += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(v & 3) << 4], t += "==") : 4 == u && (t += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(v & 15) << 2], t += "=");
          n.src = r + t;
          f(n);
        }
      };
      n.src = l;
      Bc(() => {
        f(n);
      });
    })});
    var b = h.canvas;
    b && (document.addEventListener("pointerlockchange", a, !1), h.elementPointerLock && b.addEventListener("click", c => {
      !Cc && h.canvas.requestPointerLock && (h.canvas.requestPointerLock(), c.preventDefault());
    }, !1));
  }
}
function Gc(a, b, c, d) {
  if (b && h.ctx && a == h.canvas) {
    return h.ctx;
  }
  var e;
  if (b) {
    var f = {antialias:!1, alpha:!1, ab:"undefined" != typeof WebGL2RenderingContext ? 2 : 1};
    if (d) {
      for (var g in d) {
        f[g] = d[g];
      }
    }
    if ("undefined" != typeof Hc && (e = Ic(a, f))) {
      var l = Jc[e].V;
    }
  } else {
    l = a.getContext("2d");
  }
  if (!l) {
    return null;
  }
  c && (b || k("undefined" == typeof L, "cannot set in module if GLctx is used, but we are a non-GL context that would replace it"), h.ctx = l, b && Kc(e), Dc.forEach(n => n()), Ob());
  return l;
}
function Fc(a) {
  return {jpg:"image/jpeg", jpeg:"image/jpeg", png:"image/png", bmp:"image/bmp", ogg:"audio/ogg", wav:"audio/wav", mp3:"audio/mpeg"}[a.slice(a.lastIndexOf(".") + 1)];
}
var Nb = {}, Ec, M = 12288, Lc = !1, Mc = 0, Nc = 0, Oc = 0, O = {alpha:!1, depth:!1, stencil:!1, antialias:!1}, Pc = {}, Qc, L, Rc = a => {
  var b = a.getExtension("ANGLE_instanced_arrays");
  b && (a.vertexAttribDivisor = (c, d) => b.vertexAttribDivisorANGLE(c, d), a.drawArraysInstanced = (c, d, e, f) => b.drawArraysInstancedANGLE(c, d, e, f), a.drawElementsInstanced = (c, d, e, f, g) => b.drawElementsInstancedANGLE(c, d, e, f, g));
}, Sc = a => {
  var b = a.getExtension("OES_vertex_array_object");
  b && (a.createVertexArray = () => b.createVertexArrayOES(), a.deleteVertexArray = c => b.deleteVertexArrayOES(c), a.bindVertexArray = c => b.bindVertexArrayOES(c), a.isVertexArray = c => b.isVertexArrayOES(c));
}, Tc = a => {
  var b = a.getExtension("WEBGL_draw_buffers");
  b && (a.drawBuffers = (c, d) => b.drawBuffersWEBGL(c, d));
}, Uc = a => {
  var b = "ANGLE_instanced_arrays EXT_blend_minmax EXT_disjoint_timer_query EXT_frag_depth EXT_shader_texture_lod EXT_sRGB OES_element_index_uint OES_fbo_render_mipmap OES_standard_derivatives OES_texture_float OES_texture_half_float OES_texture_half_float_linear OES_vertex_array_object WEBGL_color_buffer_float WEBGL_depth_texture WEBGL_draw_buffers EXT_color_buffer_float EXT_conservative_depth EXT_disjoint_timer_query_webgl2 EXT_texture_norm16 NV_shader_noperspective_interpolation WEBGL_clip_cull_distance EXT_clip_control EXT_color_buffer_half_float EXT_depth_clamp EXT_float_blend EXT_polygon_offset_clamp EXT_texture_compression_bptc EXT_texture_compression_rgtc EXT_texture_filter_anisotropic KHR_parallel_shader_compile OES_texture_float_linear WEBGL_blend_func_extended WEBGL_compressed_texture_astc WEBGL_compressed_texture_etc WEBGL_compressed_texture_etc1 WEBGL_compressed_texture_s3tc WEBGL_compressed_texture_s3tc_srgb WEBGL_debug_renderer_info WEBGL_debug_shaders WEBGL_lose_context WEBGL_multi_draw WEBGL_polygon_mode".split(" ");
  return (a.getSupportedExtensions() || []).filter(c => b.includes(c));
}, Vc = 1, Wc = [], Xc = {}, P = [], Yc = [], Zc = [], $c = [], Q = [], ad = [], Jc = [], R = [], bd = [], cd = [], dd = [], ed = [1, 1, 2, 2, 4, 4, 4, 2, 3, 4, 8], fd = {}, gd = {}, hd = 4, jd = 0, kd = a => {
  for (var b = Vc++, c = a.length; c < b; c++) {
    a[c] = null;
  }
  for (; a[b];) {
    b = Vc++;
  }
  return b;
}, ld = (a, b, c, d) => {
  for (var e = 0; e < a; e++) {
    var f = L[c](), g = f && kd(d);
    f ? (f.name = g, d[g] = f) : T ||= 1282;
    z[b + 4 * e >> 2] = g;
  }
}, md = a => {
  a = 32 - Math.clz32(0 === a ? 0 : a - 1);
  var b = V.qa[a];
  if (b) {
    return b;
  }
  b = L.getParameter(34965);
  V.qa[a] = L.createBuffer();
  L.bindBuffer(34963, V.qa[a]);
  L.bufferData(34963, 1 << a, 35048);
  L.bindBuffer(34963, b);
  return V.qa[a];
}, od = a => {
  nd = !1;
  for (var b = 0; b < V.bb; ++b) {
    var c = V.W[b];
    if (c.ja && c.enabled) {
      k(a || !L.la, "must use array buffers when using element buffer");
      nd = !0;
      var d = c.size;
      var e = c.type, f = c.stride;
      d = 0 < f ? a * f : d * ed[e - 5120] * a;
      e = 32 - Math.clz32(0 === d ? 0 : d - 1);
      f = V.ra[e];
      var g = V.da[e];
      V.da[e] = V.da[e] + 1 & 63;
      var l = f[g];
      l ? e = l : (l = L.getParameter(34964), f[g] = L.createBuffer(), L.bindBuffer(34962, f[g]), L.bufferData(34962, 1 << e, 35048), L.bindBuffer(34962, l), e = f[g]);
      L.bindBuffer(34962, e);
      L.bufferSubData(34962, 0, y.subarray(c.oa, c.oa + d));
      c.pb.call(L, b, c.size, c.type, c.gb, c.stride, 0);
    }
  }
}, Ic = (a, b) => {
  a.G || (a.G = a.getContext, a.getContext = function(d, e) {
    e = a.G(d, e);
    return "webgl" == d == e instanceof WebGLRenderingContext ? e : null;
  });
  var c = 1 < b.ab ? a.getContext("webgl2", b) : a.getContext("webgl", b);
  return c ? pd(c, b) : 0;
}, pd = (a, b) => {
  var c = kd(Jc), d = {handle:c, attributes:b, version:b.ab, V:a};
  a.canvas && (a.canvas.sa = d);
  Jc[c] = d;
  if ("undefined" == typeof b.bc || b.bc) {
    if ((a = d) || (a = V), !a.qc) {
      a.qc = !0;
      b = a.V;
      b.vd = b.getExtension("WEBGL_multi_draw");
      b.ec = b.getExtension("EXT_polygon_offset_clamp");
      b.dc = b.getExtension("EXT_clip_control");
      b.Oc = b.getExtension("WEBGL_polygon_mode");
      Rc(b);
      Sc(b);
      Tc(b);
      b.gd = b.getExtension("WEBGL_draw_instanced_base_vertex_base_instance");
      b.sd = b.getExtension("WEBGL_multi_draw_instanced_base_vertex_base_instance");
      2 <= a.version && (b.J = b.getExtension("EXT_disjoint_timer_query_webgl2"));
      if (2 > a.version || !b.J) {
        b.J = b.getExtension("EXT_disjoint_timer_query");
      }
      for (var e of Uc(b)) {
        e.includes("lose_context") || e.includes("debug") || b.getExtension(e);
      }
    }
  }
  d.bb = d.V.getParameter(34921);
  d.W = [];
  for (e = 0; e < d.bb; e++) {
    d.W[e] = {enabled:!1, ja:!1, size:0, type:0, gb:0, stride:0, oa:0, pb:null};
  }
  d.da = [];
  d.Ma = [];
  d.da.length = d.Ma.length = 22;
  d.ra = [];
  d.ya = [];
  d.ra.length = d.ya.length = 22;
  d.qa = [];
  d.qa.length = 22;
  for (e = 0; 21 >= e; ++e) {
    d.qa[e] = null;
    d.da[e] = d.Ma[e] = 0;
    d.ra[e] = [];
    d.ya[e] = [];
    a = d.ra[e];
    b = d.ya[e];
    a.length = b.length = 64;
    for (var f = 0; 64 > f; ++f) {
      a[f] = b[f] = null;
    }
  }
  return c;
}, Kc = a => {
  V = Jc[a];
  h.ctx = L = V?.V;
}, Hc = {}, T, V, nd, rd = a => {
  var b = ub(a) + 1, c = qd(b);
  c && K(a, c, b);
  return c;
}, Id = (a, b, c, d, e) => {
  function f() {
    return g < sd ? (xc(), !1) : !0;
  }
  k(!td, "emscripten_set_main_loop: there can only be one main loop function at once: call emscripten_cancel_main_loop to cancel the previous one before setting a new one with different parameters.");
  td = a;
  ud = d;
  var g = sd;
  vd = !1;
  wd = function() {
    if (!sa) {
      if (0 < xd.length) {
        var l = xd.shift();
        l.qd(l.Pc);
        if (yd) {
          var n = yd, r = 0 == n % 1 ? n - 1 : Math.floor(n);
          yd = l.ad ? r : (8 * n + (r + 0.5)) / 9;
        }
        h.setStatus && (l = h.statusMessage || "Please wait...", n = yd ?? 0, r = zd.jd ?? 0, n ? n < r ? h.setStatus("{message} ({expected - remaining}/{expected})") : h.setStatus(l) : h.setStatus(""));
        f() && setTimeout(wd, 0);
      } else {
        if (f()) {
          if (Ad = Ad + 1 | 0, 1 == Bd && 1 < Cd && 0 != Ad % Cd) {
            Dd();
          } else {
            0 == Bd && (Ed = performance.now(), h.ctx && Fa("Looks like you are rendering without using requestAnimationFrame for the main loop. You should use 0 for the frame rate in emscripten_set_main_loop in order to use requestAnimationFrame, as that can greatly improve your frame rates!"));
            a: {
              if (!sa) {
                for (l of Fd) {
                  if (!1 === l()) {
                    break a;
                  }
                }
                yc(a);
                for (n of Gd) {
                  n();
                }
                wa();
              }
            }
            f() && Dd();
          }
        }
      }
    }
  };
  e || (0 < b ? Hd(0, 1000.0 / b) : Hd(1, 1), Dd());
  if (c) {
    throw "unwind";
  }
}, vd = !1, Dd = null, sd = 0, td = null, ud = 0, Bd = 0, Cd = 0, Ad = 0, xd = [], Fd = [], Gd = [];
function Jd() {
  Dd = null;
  sd++;
}
function Kd() {
  sd++;
  var a = Bd, b = Cd, c = td;
  td = null;
  Id(c, 0, !1, ud, !0);
  Hd(a, b);
  Dd();
}
var Ld = 0;
function Md(a) {
  if (globalThis.requestAnimationFrame) {
    requestAnimationFrame(a);
  } else {
    var b = Date.now();
    if (0 === Ld) {
      Ld = b + 1000 / 60;
    } else {
      for (; b + 2 >= Ld;) {
        Ld += 1000 / 60;
      }
    }
    setTimeout(a, Math.max(Ld - b, 0));
  }
}
var zd = {}, wd, yd, Ed, Nd, Hd = (a, b) => {
  Bd = a;
  Cd = b;
  if (td) {
    if (vd ||= !0, 0 == a) {
      Dd = function() {
        setTimeout(wd, Math.max(0, Ed + b - performance.now()) | 0);
      };
    } else if (1 == a) {
      Dd = function() {
        Md(wd);
      };
    } else {
      k(2 == a);
      if (!Nd) {
        if (globalThis.setImmediate) {
          Nd = setImmediate;
        } else {
          var c = [];
          addEventListener("message", d => {
            if ("setimmediate" === d.data || "setimmediate" === d.data.target) {
              d.stopPropagation(), c.shift()();
            }
          }, !0);
          Nd = d => {
            c.push(d);
            if (ca) {
              let e;
              (e = h).setImmediates ?? (e.setImmediates = []);
              h.setImmediates.push(d);
              postMessage({target:"setimmediate"});
            } else {
              postMessage("setimmediate", "*");
            }
          };
        }
      }
      Dd = function() {
        Nd(wd);
      };
    }
  } else {
    m("emscripten_set_main_loop_timing: Cannot set timing mode for main loop since a main loop does not exist! Call emscripten_set_main_loop first to set one up.");
  }
}, Od = [], Pd = (a, b) => {
  k(Array.isArray(Od));
  k(0 == b % 16);
  Od.length = 0;
  for (var c; c = y[a++];) {
    var d = String.fromCharCode(c), e = ["d", "f", "i", "p"];
    e.push("j");
    k(e.includes(d), `Invalid character ${c}("${d}") in readEmAsmArgs! Use only [${e}], and do not specify "v" for void return argument.`);
    d = 105 != c;
    d &= 112 != c;
    b += d && b % 8 ? 4 : 0;
    Od.push(112 == c ? q[b >> 2] : 106 == c ? Na[b >> 3] : 105 == c ? z[b >> 2] : Ma[b >> 3]);
    b += d ? 8 : 4;
  }
  return Od;
}, Rd = (a, b, c) => {
  b = Pd(b, c);
  k(Qd.hasOwnProperty(a), `No EM_ASM constant found at address ${a}.  The loaded WebAssembly file is likely out of sync with the generated JavaScript.`);
  return Qd[a](...b);
};
function Sd(a) {
  var b = X.C[a];
  b.target.removeEventListener(b.u, b.zb, b.H);
  X.C.splice(a, 1);
}
function Td(a, b, c) {
  function d(f, g) {
    if (f.length != g.length) {
      return !1;
    }
    for (var l in f) {
      if (f[l] != g[l]) {
        return !1;
      }
    }
    return !0;
  }
  for (var e of X.aa) {
    if (e.lb == a && d(e.tb, c)) {
      return;
    }
  }
  X.aa.push({lb:a, Ib:b, tb:c});
  X.aa.sort((f, g) => f.Ib - g.Ib);
}
function Ud(a) {
  X.aa = X.aa.filter(b => b.lb != a);
}
function Vd() {
  return navigator.userActivation ? navigator.userActivation.isActive : X.Va && X.Vb.Aa;
}
function Wd() {
  if (Vd()) {
    var a = X.aa;
    X.aa = [];
    for (var b of a) {
      b.lb(...b.tb);
    }
  }
}
function Xd(a) {
  if (!a.target) {
    return m("registerOrRemoveHandler: the target element for event handler registration does not exist, when processing the following event handler registration:"), console.dir(a), -4;
  }
  if (a.B) {
    a.zb = function(c) {
      ++X.Va;
      X.Vb = a;
      Wd();
      a.M(c);
      Wd();
      --X.Va;
    }, a.target.addEventListener(a.u, a.zb, a.H), X.C.push(a);
  } else {
    for (var b = 0; b < X.C.length; ++b) {
      X.C[b].target == a.target && X.C[b].u == a.u && Sd(b--);
    }
  }
  return 0;
}
function Yd(a) {
  return a ? a == window ? "#window" : a == screen ? "#screen" : a?.nodeName || "" : "";
}
var X = {Rc:0, Ta:0, Ya:0, Ka:0, rb:0, nb:0, Ra:0, ed:0, xd:0, cd:0, Sa:0, hb:0, qb:0, mb:0, zd() {
  for (; X.C.length;) {
    Sd(X.C.length - 1);
  }
  X.aa = [];
}, Va:0, aa:[], C:[], Gc:(a, b) => {
  for (var c = 0; c < X.C.length; ++c) {
    X.C[c].target != a || b && b != X.C[c].u || Sd(c--);
  }
}, Ad(a) {
  let b = !1;
  for (let c = 0; c < X.C.length; ++c) {
    const d = X.C[c];
    d.target === a.target && d.F === a.F && d.B === a.B && d.I === a.I && (Sd(c--), b = !0);
  }
  return b ? 0 : -5;
}, fullscreenEnabled() {
  return document.fullscreenEnabled || document.webkitFullscreenEnabled;
}}, Zd = [0, globalThis.document ?? 0, globalThis.window ?? 0], Y = a => {
  a = 2 < a ? D(a) : a;
  return Zd[a] || globalThis.document?.querySelector(a);
}, ae = a => {
  var b = ub(a) + 1, c = $d(b);
  K(a, c, b);
  return c;
}, ce = a => {
  var b = sc(), c = $d(8), d = c + 4;
  a = ae(a.id);
  if (a = Y(a)) {
    z[c >> 2] = a.width, z[d >> 2] = a.height;
  }
  c = [z[c >> 2], z[d >> 2]];
  be(b);
  return c;
}, de = (a, b, c) => {
  a = Y(a);
  if (!a) {
    return -4;
  }
  a.width = b;
  a.height = c;
  return 0;
}, ee = (a, b, c) => {
  if (a.$c) {
    var d = sc();
    a = ae(a.id);
    de(a, b, c);
    be(d);
  } else {
    a.width = b, a.height = c;
  }
}, fe = {}, he = a => {
  function b() {
    Ac() || (document.removeEventListener("fullscreenchange", b), document.removeEventListener("webkitfullscreenchange", b), ee(a, d, e), a.style.width = f, a.style.height = g, a.style.backgroundColor = l, n || (document.body.style.backgroundColor = "white"), document.body.style.backgroundColor = n, a.style.paddingLeft = r, a.style.paddingRight = t, a.style.paddingTop = v, a.style.paddingBottom = u, a.style.marginLeft = H, a.style.marginRight = N, a.style.marginTop = S, a.style.marginBottom = ba, 
    document.body.style.margin = U, document.documentElement.style.overflow = ea, document.body.scroll = zc, a.style.ba = W, a.sa && a.sa.V.viewport(0, 0, d, e), fe.Ba && ge(fe.Ba, 37, 0, fe.ub));
  }
  var c = ce(a), d = c[0], e = c[1], f = a.style.width, g = a.style.height, l = a.style.backgroundColor, n = document.body.style.backgroundColor, r = a.style.paddingLeft, t = a.style.paddingRight, v = a.style.paddingTop, u = a.style.paddingBottom, H = a.style.marginLeft, N = a.style.marginRight, S = a.style.marginTop, ba = a.style.marginBottom, U = document.body.style.margin, ea = document.documentElement.style.overflow, zc = document.body.scroll, W = a.style.ba;
  document.addEventListener("fullscreenchange", b);
  document.addEventListener("webkitfullscreenchange", b);
}, ie = (a, b, c) => {
  a.style.paddingLeft = a.style.paddingRight = c + "px";
  a.style.paddingTop = a.style.paddingBottom = b + "px";
}, je = a => 0 > Zd.indexOf(a) ? a.getBoundingClientRect() : {left:0, top:0}, ke = (a, b) => {
  if (0 != b.jb || 0 != b.Na) {
    he(a);
    var c = b.Jc ? innerWidth : screen.width, d = b.Jc ? innerHeight : screen.height, e = je(a), f = e.width;
    e = e.height;
    var g = ce(a), l = g[0];
    g = g[1];
    3 == b.jb ? (ie(a, (d - e) / 2, (c - f) / 2), c = f, d = e) : 2 == b.jb && (c * g < l * d ? (f = g * c / l, ie(a, (d - f) / 2, 0), d = f) : (f = l * d / g, ie(a, 0, (c - f) / 2), c = f));
    var n;
    (n = a.style).backgroundColor || (n.backgroundColor = "black");
    let r;
    (r = document.body.style).backgroundColor || (r.backgroundColor = "black");
    a.style.width = c + "px";
    a.style.height = d + "px";
    1 == b.hc && (a.style.ba = "optimizeSpeed", a.style.ba = "-moz-crisp-edges", a.style.ba = "-o-crisp-edges", a.style.ba = "-webkit-optimize-contrast", a.style.ba = "optimize-contrast", a.style.ba = "crisp-edges", a.style.ba = "pixelated");
    n = 2 == b.Na ? devicePixelRatio : 1;
    0 != b.Na && (c = c * n | 0, d = d * n | 0, ee(a, c, d), a.sa && a.sa.V.viewport(0, 0, c, d));
  }
  if (a.requestFullscreen) {
    a.requestFullscreen();
  } else if (a.webkitRequestFullscreen) {
    a.webkitRequestFullscreen(Element.ALLOW_KEYBOARD_INPUT);
  } else {
    return X.fullscreenEnabled() ? -3 : -1;
  }
  fe = b;
  b.Ba && ge(b.Ba, 37, 0, b.ub);
  return 0;
}, le = a => {
  if (a.requestPointerLock) {
    a.requestPointerLock();
  } else {
    return document.body.requestPointerLock ? -3 : -1;
  }
  return 0;
}, me = (a, b) => {
  Ma[a >> 3] = b.timestamp;
  for (var c = 0; c < b.axes.length; ++c) {
    Ma[a + 8 * c + 16 >> 3] = b.axes[c];
  }
  for (c = 0; c < b.buttons.length; ++c) {
    Ma[a + 8 * c + 528 >> 3] = "object" == typeof b.buttons[c] ? b.buttons[c].value : b.buttons[c];
  }
  for (c = 0; c < b.buttons.length; ++c) {
    x[a + c + 1040] = "object" == typeof b.buttons[c] ? b.buttons[c].pressed : 1 == b.buttons[c];
  }
  x[a + 1104] = b.connected;
  z[a + 1108 >> 2] = b.index;
  z[a + 8 >> 2] = b.axes.length;
  z[a + 12 >> 2] = b.buttons.length;
  K(b.id, a + 1112, 64);
  K(b.mapping, a + 1176, 64);
}, ne = a => L.activeTexture(a), oe = (a, b) => {
  L.attachShader(P[a], Q[b]);
}, pe = (a, b, c) => {
  L.bindAttribLocation(P[a], b, D(c));
}, qe = (a, b) => {
  if (b && !Wc[b]) {
    var c = L.createBuffer();
    c.name = b;
    Wc[b] = c;
  }
  34962 == a ? L.ka = b : 34963 == a && (L.la = b);
  35051 == a ? L.Qa = b : 35052 == a && (L.T = b);
  L.bindBuffer(a, Wc[b]);
}, re = (a, b) => {
  L.bindFramebuffer(a, Yc[b]);
}, se = (a, b) => {
  L.bindRenderbuffer(a, Zc[b]);
}, te = (a, b) => {
  L.bindTexture(a, $c[b]);
}, ue = a => {
  L.bindVertexArray(ad[a]);
  a = L.getParameter(34965);
  L.la = a ? a.name | 0 : 0;
}, ve = (a, b, c, d) => L.blendFuncSeparate(a, b, c, d), we = (a, b, c, d) => {
  2 <= V.version ? c && b ? L.bufferData(a, y, d, c, b) : L.bufferData(a, b, d) : L.bufferData(a, c ? y.subarray(c, c + b) : b, d);
}, xe = a => L.checkFramebufferStatus(a), ye = a => L.clear(a), ze = (a, b, c, d) => L.clearColor(a, b, c, d), Ae = (a, b, c, d) => {
  L.colorMask(!!a, !!b, !!c, !!d);
}, Be = a => {
  L.compileShader(Q[a]);
}, Ce = () => {
  var a = kd(P), b = L.createProgram();
  b.name = a;
  b.Ja = b.Ha = b.Ia = 0;
  b.ob = 1;
  P[a] = b;
  return a;
}, De = a => {
  var b = kd(Q);
  Q[b] = L.createShader(a);
  return b;
}, Ee = a => L.cullFace(a), Fe = (a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = Wc[d];
    e && (L.deleteBuffer(e), e.name = 0, Wc[d] = null, d == L.ka && (L.ka = 0), d == L.la && (L.la = 0), d == L.Qa && (L.Qa = 0), d == L.T && (L.T = 0));
  }
}, Ge = (a, b) => {
  for (var c = 0; c < a; ++c) {
    var d = z[b + 4 * c >> 2], e = Yc[d];
    e && (L.deleteFramebuffer(e), e.name = 0, Yc[d] = null);
  }
}, He = a => {
  if (a) {
    var b = P[a];
    b ? (L.deleteProgram(b), b.name = 0, P[a] = null) : T ||= 1281;
  }
}, Ie = (a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = Zc[d];
    e && (L.deleteRenderbuffer(e), e.name = 0, Zc[d] = null);
  }
}, Je = a => {
  if (a) {
    var b = Q[a];
    b ? (L.deleteShader(b), Q[a] = null) : T ||= 1281;
  }
}, Ke = (a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = $c[d];
    e && (L.deleteTexture(e), e.name = 0, $c[d] = null);
  }
}, Le = (a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2];
    L.deleteVertexArray(ad[d]);
    ad[d] = null;
  }
}, Me = a => L.disable(a), Ne = a => {
  V.W[a].enabled = !1;
  L.disableVertexAttribArray(a);
}, Oe = (a, b, c) => {
  od(b + c);
  L.drawArrays(a, b, c);
  nd && L.bindBuffer(34962, Wc[L.ka]);
}, Pe = (a, b, c, d) => {
  L.drawArraysInstanced(a, b, c, d);
}, Qe = [], Re = (a, b) => {
  for (var c = Qe[a], d = 0; d < a; d++) {
    c[d] = z[b + 4 * d >> 2];
  }
  L.drawBuffers(c);
}, Se = (a, b, c, d) => {
  var e = 0;
  if (!L.la) {
    var f = 1 * ed[c - 5120] * b;
    var g = md(f);
    L.bindBuffer(34963, g);
    L.bufferSubData(34963, 0, y.subarray(d, d + f));
    if (0 < b) {
      for (g = 0; g < V.bb; ++g) {
        if (f = V.W[g], f.ja && f.enabled) {
          switch(c) {
            case 5121:
              e = Uint8Array;
              break;
            case 5123:
              e = Uint16Array;
              break;
            case 5125:
              e = Uint32Array;
              break;
            default:
              T ||= 1282;
              return;
          }
          e = (new e(y.buffer, d, b)).reduce((l, n) => Math.max(l, n)) + 1;
          break;
        }
      }
    }
    d = 0;
  }
  od(e);
  L.drawElements(a, b, c, d);
  nd && L.bindBuffer(34962, Wc[L.ka]);
  L.la || L.bindBuffer(34963, null);
}, Te = (a, b, c, d, e) => {
  L.drawElementsInstanced(a, b, c, d, e);
}, Ue = a => L.enable(a), Ve = a => {
  V.W[a].enabled = !0;
  L.enableVertexAttribArray(a);
}, We = () => L.finish(), Xe = a => {
  switch(a) {
    case 34962:
      a = 34964;
      break;
    case 34963:
      a = 34965;
      break;
    case 35051:
      a = 35053;
      break;
    case 35052:
      a = 35055;
      break;
    case 35982:
      a = 35983;
      break;
    case 36662:
      a = 36662;
      break;
    case 36663:
      a = 36663;
      break;
    case 35345:
      a = 35368;
  }
  return (a = L.getParameter(a)) ? a.name | 0 : 0;
}, Ye = a => {
  switch(a) {
    case 34962:
    case 34963:
    case 36662:
    case 36663:
    case 35051:
    case 35052:
    case 35882:
    case 35982:
    case 35345:
      return !0;
    default:
      return !1;
  }
}, Ze = (a, b, c, d) => {
  L.framebufferRenderbuffer(a, b, c, Zc[d]);
}, $e = (a, b, c, d, e) => {
  L.framebufferTexture2D(a, b, c, $c[d], e);
}, af = a => L.frontFace(a), bf = (a, b) => {
  ld(a, b, "createBuffer", Wc);
}, cf = (a, b) => {
  ld(a, b, "createFramebuffer", Yc);
}, df = (a, b) => {
  ld(a, b, "createRenderbuffer", Zc);
}, ef = (a, b) => {
  ld(a, b, "createTexture", $c);
}, ff = (a, b) => {
  ld(a, b, "createVertexArray", ad);
}, gf = a => L.generateMipmap(a), hf = (a, b, c, d, e, f, g, l) => {
  b = P[b];
  if (a = L[a](b, c)) {
    d = l && K(a.name, l, d), e && (z[e >> 2] = d), f && (z[f >> 2] = a.size), g && (z[g >> 2] = a.type);
  }
}, jf = (a, b) => {
  q[a >> 2] = b;
  q[a + 4 >> 2] = (b - q[a >> 2]) / 4294967296;
  var c = 0 <= b ? q[a >> 2] + 4294967296 * q[a + 4 >> 2] : q[a >> 2] + 4294967296 * z[a + 4 >> 2];
  a >>= 2;
  c != b && Fa(`writeI53ToI64() out of range: serialized JS Number ${b} to Wasm heap as bytes lo=${xa(q[a])}, hi=${xa(q[a + 1])}, which deserializes back to ${c} instead!`);
}, kf = () => {
  var a = Uc(L);
  return a = a.concat(a.map(b => "GL_" + b));
}, lf = (a, b, c) => {
  if (b) {
    var d = void 0;
    switch(a) {
      case 36346:
        d = 1;
        break;
      case 36344:
        0 != c && 1 != c && (T ||= 1280);
        return;
      case 34814:
      case 36345:
        d = 0;
        break;
      case 34466:
        var e = L.getParameter(34467);
        d = e ? e.length : 0;
        break;
      case 33309:
        if (2 > V.version) {
          T ||= 1282;
          return;
        }
        d = kf().length;
        break;
      case 33307:
      case 33308:
        if (2 > V.version) {
          T ||= 1280;
          return;
        }
        d = 33307 == a ? 3 : 0;
    }
    if (void 0 === d) {
      switch(e = L.getParameter(a), typeof e) {
        case "number":
          d = e;
          break;
        case "boolean":
          d = e ? 1 : 0;
          break;
        case "string":
          T ||= 1280;
          return;
        case "object":
          if (null === e) {
            switch(a) {
              case 34964:
              case 35725:
              case 34965:
              case 36006:
              case 36007:
              case 32873:
              case 34229:
              case 36662:
              case 36663:
              case 35053:
              case 35055:
              case 36010:
              case 35097:
              case 35869:
              case 32874:
              case 36389:
              case 35983:
              case 35368:
              case 34068:
                d = 0;
                break;
              default:
                T ||= 1280;
                return;
            }
          } else {
            if (e instanceof Float32Array || e instanceof Uint32Array || e instanceof Int32Array || e instanceof Array) {
              for (a = 0; a < e.length; ++a) {
                switch(c) {
                  case 0:
                    z[b + 4 * a >> 2] = e[a];
                    break;
                  case 2:
                    A[b + 4 * a >> 2] = e[a];
                    break;
                  case 4:
                    x[b + a] = e[a] ? 1 : 0;
                }
              }
              return;
            }
            try {
              d = e.name | 0;
            } catch (f) {
              T ||= 1280;
              m(`GL_INVALID_ENUM in glGet${c}v: Unknown object returned from WebGL getParameter(${a})! (error: ${f})`);
              return;
            }
          }
          break;
        default:
          T ||= 1280;
          m(`GL_INVALID_ENUM in glGet${c}v: Native code calling glGet${c}v(${a}) and it returns ${e} of type ${typeof e}!`);
          return;
      }
    }
    switch(c) {
      case 1:
        jf(b, d);
        break;
      case 0:
        z[b >> 2] = d;
        break;
      case 2:
        A[b >> 2] = d;
        break;
      case 4:
        x[b] = d ? 1 : 0;
    }
  } else {
    T ||= 1281;
  }
}, mf = () => {
  var a = L.getError() || T;
  T = 0;
  return a;
}, nf = (a, b, c, d) => {
  if (c) {
    b = L.getIndexedParameter(a, b);
    switch(typeof b) {
      case "boolean":
        a = b ? 1 : 0;
        break;
      case "number":
        a = b;
        break;
      case "object":
        if (null === b) {
          switch(a) {
            case 35983:
            case 35368:
              a = 0;
              break;
            default:
              T ||= 1280;
              return;
          }
        } else if (b instanceof WebGLBuffer) {
          a = b.name | 0;
        } else {
          T ||= 1280;
          return;
        }
        break;
      default:
        T ||= 1280;
        return;
    }
    switch(d) {
      case 1:
        jf(c, a);
        break;
      case 0:
        z[c >> 2] = a;
        break;
      case 2:
        A[c >> 2] = a;
        break;
      case 4:
        x[c] = a ? 1 : 0;
        break;
      default:
        p("internal emscriptenWebGLGetIndexed() error, bad type: " + d);
    }
  } else {
    T ||= 1281;
  }
}, of = (a, b) => lf(a, b, 0), pf = (a, b, c, d) => {
  a = L.getProgramInfoLog(P[a]);
  null === a && (a = "(unknown error)");
  b = 0 < b && d ? K(a, d, b) : 0;
  c && (z[c >> 2] = b);
}, qf = (a, b, c) => {
  if (c) {
    if (a >= Vc) {
      T ||= 1281;
    } else {
      if (a = P[a], 35716 == b) {
        a = L.getProgramInfoLog(a), null === a && (a = "(unknown error)"), z[c >> 2] = a.length + 1;
      } else if (35719 == b) {
        if (!a.Ja) {
          var d = L.getProgramParameter(a, 35718);
          for (b = 0; b < d; ++b) {
            a.Ja = Math.max(a.Ja, L.getActiveUniform(a, b).name.length + 1);
          }
        }
        z[c >> 2] = a.Ja;
      } else if (35722 == b) {
        if (!a.Ha) {
          for (d = L.getProgramParameter(a, 35721), b = 0; b < d; ++b) {
            a.Ha = Math.max(a.Ha, L.getActiveAttrib(a, b).name.length + 1);
          }
        }
        z[c >> 2] = a.Ha;
      } else if (35381 == b) {
        if (!a.Ia) {
          for (d = L.getProgramParameter(a, 35382), b = 0; b < d; ++b) {
            a.Ia = Math.max(a.Ia, L.getActiveUniformBlockName(a, b).length + 1);
          }
        }
        z[c >> 2] = a.Ia;
      } else {
        z[c >> 2] = L.getProgramParameter(a, b);
      }
    }
  } else {
    T ||= 1281;
  }
}, rf = (a, b, c) => {
  if (c) {
    a = R[a];
    b = 2 > V.version ? L.J.getQueryObjectEXT(a, b) : L.getQueryParameter(a, b);
    var d;
    "boolean" == typeof b ? d = b ? 1 : 0 : d = b;
    jf(c, d);
  } else {
    T ||= 1281;
  }
}, sf = (a, b, c) => {
  if (c) {
    a = L.J.getQueryObjectEXT(R[a], b);
    var d;
    "boolean" == typeof a ? d = a ? 1 : 0 : d = a;
    z[c >> 2] = d;
  } else {
    T ||= 1281;
  }
}, tf = (a, b, c, d) => {
  a = L.getShaderInfoLog(Q[a]);
  null === a && (a = "(unknown error)");
  b = 0 < b && d ? K(a, d, b) : 0;
  c && (z[c >> 2] = b);
}, uf = (a, b, c) => {
  c ? 35716 == b ? (a = L.getShaderInfoLog(Q[a]), null === a && (a = "(unknown error)"), z[c >> 2] = a ? a.length + 1 : 0) : 35720 == b ? (a = L.getShaderSource(Q[a]), z[c >> 2] = a ? a.length + 1 : 0) : z[c >> 2] = L.getShaderParameter(Q[a], b) : T ||= 1281;
}, vf = a => "]" == a.slice(-1) && a.lastIndexOf("["), wf = a => {
  var b = a.za, c = a.Mb, d;
  if (!b) {
    a.za = b = {};
    a.Lb = {};
    var e = L.getProgramParameter(a, 35718);
    for (d = 0; d < e; ++d) {
      var f = L.getActiveUniform(a, d);
      var g = f.name;
      f = f.size;
      var l = vf(g);
      l = 0 < l ? g.slice(0, l) : g;
      var n = a.ob;
      a.ob += f;
      c[l] = [f, n];
      for (g = 0; g < f; ++g) {
        b[n] = g, a.Lb[n++] = l;
      }
    }
  }
}, xf = (a, b) => {
  b = D(b);
  if (a = P[a]) {
    wf(a);
    var c = a.za, d = 0, e = b, f = vf(b);
    0 < f && (d = parseInt(b.slice(f + 1)) >>> 0, e = b.slice(0, f));
    if ((e = a.Mb[e]) && d < e[0] && (d += e[1], c[d] = c[d] || L.getUniformLocation(a, b))) {
      return d;
    }
  } else {
    T ||= 1281;
  }
  return -1;
}, Z = a => {
  var b = L.Wb;
  if (b) {
    var c = b.za[a];
    "number" == typeof c && (b.za[a] = c = L.getUniformLocation(b, b.Lb[a] + (0 < c ? `[${c}]` : "")));
    return c;
  }
  T ||= 1282;
}, yf = (a, b, c, d) => {
  if (c) {
    if (a = P[a], wf(a), a = L.getUniform(a, Z(b)), "number" == typeof a || "boolean" == typeof a) {
      switch(d) {
        case 0:
          z[c >> 2] = a;
          break;
        case 2:
          A[c >> 2] = a;
      }
    } else {
      for (b = 0; b < a.length; b++) {
        switch(d) {
          case 0:
            z[c + 4 * b >> 2] = a[b];
            break;
          case 2:
            A[c + 4 * b >> 2] = a[b];
        }
      }
    }
  } else {
    T ||= 1281;
  }
}, zf = (a, b, c, d) => {
  if (c) {
    if (V.W[a].enabled && m("glGetVertexAttrib*v on client-side array: not supported, bad data returned"), a = L.getVertexAttrib(a, b), 34975 == b) {
      z[c >> 2] = a && a.name;
    } else if ("number" == typeof a || "boolean" == typeof a) {
      switch(d) {
        case 0:
          z[c >> 2] = a;
          break;
        case 2:
          A[c >> 2] = a;
          break;
        case 5:
          z[c >> 2] = Math.fround(a);
      }
    } else {
      for (b = 0; b < a.length; b++) {
        switch(d) {
          case 0:
            z[c + 4 * b >> 2] = a[b];
            break;
          case 2:
            A[c + 4 * b >> 2] = a[b];
            break;
          case 5:
            z[c + 4 * b >> 2] = Math.fround(a[b]);
        }
      }
    }
  } else {
    T ||= 1281;
  }
}, Af = (a, b, c) => {
  zf(a, b, c, 0);
}, Bf = a => (a = ad[a]) ? L.isVertexArray(a) : 0, Cf = a => {
  a = P[a];
  L.linkProgram(a);
  a.za = 0;
  a.Mb = {};
}, Df = (a, b) => {
  3317 == a ? hd = b : 3314 == a && (jd = b);
  L.pixelStorei(a, b);
}, Ef = a => {
  a -= 5120;
  return 0 == a ? x : 1 == a ? y : 2 == a ? Ka : 4 == a ? z : 6 == a ? A : 5 == a || 28922 == a || 28520 == a || 30779 == a || 30782 == a ? q : La;
}, Ff = (a, b, c, d, e) => {
  a = Ef(a);
  b = d * ((jd || c) * ({5:3, 6:4, 8:2, 29502:3, 29504:4, 26917:2, 26918:2, 29846:3, 29847:4}[b - 6402] || 1) * a.BYTES_PER_ELEMENT + hd - 1 & -hd);
  return a.subarray(e >>> 31 - Math.clz32(a.BYTES_PER_ELEMENT), e + b >>> 31 - Math.clz32(a.BYTES_PER_ELEMENT));
}, Gf = (a, b, c, d, e, f, g) => {
  if (2 <= V.version) {
    if (L.Qa) {
      L.readPixels(a, b, c, d, e, f, g);
    } else {
      var l = Ef(f);
      g >>>= 31 - Math.clz32(l.BYTES_PER_ELEMENT);
      L.readPixels(a, b, c, d, e, f, l, g);
    }
  } else {
    (l = Ff(f, e, c, d, g)) ? L.readPixels(a, b, c, d, e, f, l) : T ||= 1280;
  }
}, Hf = (a, b, c, d) => L.renderbufferStorage(a, b, c, d), If = (a, b, c, d) => {
  for (var e = "", f = 0; f < b; ++f) {
    e += D(q[c + 4 * f >> 2], d ? q[d + 4 * f >> 2] : void 0);
  }
  L.shaderSource(Q[a], e);
}, Jf = (a, b, c) => L.stencilFunc(a, b, c), Kf = a => L.stencilMask(a), Lf = (a, b, c) => L.stencilOp(a, b, c), Mf = (a, b, c, d) => L.stencilOpSeparate(a, b, c, d), Nf = (a, b, c, d, e, f, g, l, n) => {
  if (2 <= V.version) {
    if (L.T) {
      L.texImage2D(a, b, c, d, e, f, g, l, n);
      return;
    }
    if (n) {
      var r = Ef(l);
      n >>>= 31 - Math.clz32(r.BYTES_PER_ELEMENT);
      L.texImage2D(a, b, c, d, e, f, g, l, r, n);
      return;
    }
  }
  r = n ? Ff(l, g, d, e, n) : null;
  L.texImage2D(a, b, c, d, e, f, g, l, r);
}, Of = (a, b, c) => L.texParameteri(a, b, c), Pf = (a, b, c, d, e, f, g, l, n) => {
  if (2 <= V.version) {
    if (L.T) {
      L.texSubImage2D(a, b, c, d, e, f, g, l, n);
      return;
    }
    if (n) {
      var r = Ef(l);
      L.texSubImage2D(a, b, c, d, e, f, g, l, r, n >>> 31 - Math.clz32(r.BYTES_PER_ELEMENT));
      return;
    }
  }
  n = n ? Ff(l, g, e, f, n) : null;
  L.texSubImage2D(a, b, c, d, e, f, g, l, n);
}, Qf = [], Rf = (a, b) => {
  L.uniform1i(Z(a), b);
}, Sf = [], Tf = (a, b, c) => {
  if (2 <= V.version) {
    b && L.uniform2fv(Z(a), A, c >> 2, 2 * b);
  } else {
    if (144 >= b) {
      b *= 2;
      for (var d = Qf[b], e = 0; e < b; e += 2) {
        d[e] = A[c + 4 * e >> 2], d[e + 1] = A[c + (4 * e + 4) >> 2];
      }
    } else {
      d = A.subarray(c >> 2, c + 8 * b >> 2);
    }
    L.uniform2fv(Z(a), d);
  }
}, Uf = (a, b, c) => {
  if (2 <= V.version) {
    b && L.uniform4fv(Z(a), A, c >> 2, 4 * b);
  } else {
    if (72 >= b) {
      var d = Qf[4 * b], e = A;
      c >>= 2;
      b *= 4;
      for (var f = 0; f < b; f += 4) {
        var g = c + f;
        d[f] = e[g];
        d[f + 1] = e[g + 1];
        d[f + 2] = e[g + 2];
        d[f + 3] = e[g + 3];
      }
    } else {
      d = A.subarray(c >> 2, c + 16 * b >> 2);
    }
    L.uniform4fv(Z(a), d);
  }
}, Vf = a => {
  a = P[a];
  L.useProgram(a);
  L.Wb = a;
}, Wf = (a, b) => {
  L.vertexAttribDivisor(a, b);
}, Xf = (a, b, c, d, e, f) => {
  var g = V.W[a];
  L.ka ? (g.ja = !1, L.vertexAttribPointer(a, b, c, !!d, e, f)) : (g.size = b, g.type = c, g.gb = d, g.stride = e, g.oa = f, g.ja = !0, g.pb = function(l, n, r, t, v, u) {
    this.vertexAttribPointer(l, n, r, t, v, u);
  });
}, Yf = (a, b, c, d) => L.viewport(a, b, c, d), Zf = (a, b) => {
  if (!X.fullscreenEnabled()) {
    return -1;
  }
  a = Y(a);
  return a ? a.requestFullscreen || a.webkitRequestFullscreen ? Vd() ? ke(a, b) : b.$b ? (Td(ke, 1, [a, b]), 1) : -2 : -3 : -4;
}, $f = () => {
  try {
    if (navigator.getGamepads) {
      return (X.ma = navigator.getGamepads()) ? 0 : -1;
    }
  } catch (a) {
    m(`navigator.getGamepads() exists, but failed to execute with exception ${a}. Disabling Gamepad access.`), navigator.getGamepads = null;
  }
  return -1;
}, ag = (a, b) => {
  var c = {target:Y(2), u:"beforeunload", F:28, I:a, B:b, M:d => {
    var e = ge(b, 28, 0, a);
    e &&= D(e);
    if (e) {
      return d.preventDefault(), d.returnValue = e;
    }
  }, H:!0};
  return Xd(c);
}, bg = (a, b, c, d, e, f) => {
  X.Ra || (X.Ra = qd(256));
  a = {target:Y(a), u:f, F:e, I:b, B:d, M:g => {
    var l = g.target.id ? g.target.id : "", n = X.Ra;
    K(Yd(g.target), n + 0, 128);
    K(l, n + 128, 128);
    ge(d, e, n, b) && g.preventDefault();
  }, H:c};
  return Xd(a);
}, cg = (a, b, c, d, e) => {
  X.Sa || (X.Sa = qd(276));
  return Xd({target:a, u:e, F:19, I:b, B:d, M:f => {
    var g = X.Sa, l = Ac(), n = !!l;
    x[g] = n;
    x[g + 1] = X.fullscreenEnabled();
    var r = n ? l : X.Dc, t = r?.id || "";
    K(Yd(r), g + 2, 128);
    K(t, g + 130, 128);
    z[g + 260 >> 2] = r ? r.clientWidth : 0;
    z[g + 264 >> 2] = r ? r.clientHeight : 0;
    z[g + 268 >> 2] = screen.width;
    z[g + 272 >> 2] = screen.height;
    n && (X.Dc = l);
    ge(d, 19, g, b) && f.preventDefault();
  }, H:c});
}, dg = (a, b, c, d, e) => {
  X.Ta || (X.Ta = qd(1240));
  b = {target:Y(2), Aa:!0, u:e, F:d, I:a, B:c, M:f => {
    var g = X.Ta;
    me(g, f.gamepad);
    ge(c, d, g, a) && f.preventDefault();
  }, H:b};
  return Xd(b);
}, eg = (a, b, c, d, e, f) => {
  X.Ya || (X.Ya = qd(160));
  a = {target:Y(a), u:f, F:e, I:b, B:d, M:g => {
    k(g);
    var l = X.Ya;
    Ma[l >> 3] = g.timeStamp;
    var n = l >> 2;
    z[n + 2] = g.location;
    x[l + 12] = g.ctrlKey;
    x[l + 13] = g.shiftKey;
    x[l + 14] = g.altKey;
    x[l + 15] = g.metaKey;
    x[l + 16] = g.repeat;
    z[n + 5] = g.charCode;
    z[n + 6] = g.keyCode;
    z[n + 7] = g.which;
    K(g.key || "", l + 32, 32);
    K(g.code || "", l + 64, 32);
    K(g.char || "", l + 96, 32);
    K(g.locale || "", l + 128, 32);
    ge(d, e, l, b) && g.preventDefault();
  }, H:c};
  return Xd(a);
}, fg = (a, b, c) => {
  k(0 == a % 4);
  Ma[a >> 3] = b.timeStamp;
  var d = a >> 2;
  z[d + 2] = b.screenX;
  z[d + 3] = b.screenY;
  z[d + 4] = b.clientX;
  z[d + 5] = b.clientY;
  x[a + 24] = b.ctrlKey;
  x[a + 25] = b.shiftKey;
  x[a + 26] = b.altKey;
  x[a + 27] = b.metaKey;
  Ka[2 * d + 14] = b.button;
  Ka[2 * d + 15] = b.buttons;
  z[d + 8] = b.movementX;
  z[d + 9] = b.movementY;
  a = je(c);
  z[d + 10] = b.clientX - (a.left | 0);
  z[d + 11] = b.clientY - (a.top | 0);
}, gg = (a, b, c, d, e, f) => {
  X.Ka || (X.Ka = qd(64));
  a = Y(a);
  return Xd({target:a, Aa:"mousemove" != f && "mouseenter" != f && "mouseleave" != f, u:f, F:e, I:b, B:d, M:g => {
    fg(X.Ka, g, a);
    ge(d, e, X.Ka, b) && g.preventDefault();
  }, H:c});
}, hg = (a, b, c, d) => {
  X.hb || (X.hb = qd(257));
  return Xd({target:a, u:"pointerlockchange", F:20, I:b, B:d, M:e => {
    var f = X.hb, g = document.pointerLockElement;
    x[f] = !!g;
    var l = g?.id || "";
    K(Yd(g), f + 1, 128);
    K(l, f + 129, 128);
    ge(d, 20, f, b) && e.preventDefault();
  }, H:c});
}, ig = (a, b, c, d) => {
  X.nb || (X.nb = qd(36));
  a = Y(a);
  return Xd({target:a, u:"resize", F:10, I:b, B:d, M:e => {
    if (e.target == a) {
      var f = document.body;
      if (f) {
        var g = X.nb;
        z[g >> 2] = 0;
        z[g + 4 >> 2] = f.clientWidth;
        z[g + 8 >> 2] = f.clientHeight;
        z[g + 12 >> 2] = innerWidth;
        z[g + 16 >> 2] = innerHeight;
        z[g + 20 >> 2] = outerWidth;
        z[g + 24 >> 2] = outerHeight;
        z[g + 28 >> 2] = pageXOffset | 0;
        z[g + 32 >> 2] = pageYOffset | 0;
        ge(d, 10, g, b) && e.preventDefault();
      }
    }
  }, H:c});
}, jg = (a, b, c, d, e, f) => {
  X.mb || (X.mb = qd(1552));
  a = Y(a);
  return Xd({target:a, Aa:"touchstart" == f || "touchend" == f, u:f, F:e, I:b, B:d, M:g => {
    k(g);
    var l = {}, n = g.touches;
    for (var r of n) {
      r.Cb = r.Gb = 0, l[r.identifier] = r;
    }
    for (var t of g.changedTouches) {
      t.Cb = 1, l[t.identifier] = t;
    }
    for (var v of g.targetTouches) {
      l[v.identifier].Gb = 1;
    }
    n = X.mb;
    Ma[n >> 3] = g.timeStamp;
    x[n + 12] = g.ctrlKey;
    x[n + 13] = g.shiftKey;
    x[n + 14] = g.altKey;
    x[n + 15] = g.metaKey;
    r = n + 16;
    t = je(a);
    v = 0;
    for (let u of Object.values(l)) {
      if (l = r >> 2, z[l] = u.identifier, z[l + 1] = u.screenX, z[l + 2] = u.screenY, z[l + 3] = u.clientX, z[l + 4] = u.clientY, z[l + 5] = u.pageX, z[l + 6] = u.pageY, x[r + 28] = u.Cb, x[r + 29] = u.Gb, z[l + 8] = u.clientX - (t.left | 0), z[l + 9] = u.clientY - (t.top | 0), r += 48, 31 < ++v) {
        break;
      }
    }
    z[n + 8 >> 2] = v;
    ge(d, e, n, b) && g.preventDefault();
  }, H:c});
}, kg = (a, b, c) => {
  var d = Zd[1];
  X.qb || (X.qb = qd(8));
  return Xd({target:d, u:"visibilitychange", F:21, I:a, B:c, M:e => {
    var f = X.qb, g = ["hidden", "visible", "prerender", "unloaded"].indexOf(document.visibilityState);
    x[f] = document.hidden;
    z[f + 4 >> 2] = g;
    ge(c, 21, f, a) && e.preventDefault();
  }, H:b});
}, lg = (a, b, c, d) => {
  X.rb || (X.rb = qd(96));
  return Xd({target:a, Aa:!0, u:"wheel", F:9, I:b, B:d, M:e => {
    var f = X.rb;
    fg(f, e, a);
    Ma[f + 64 >> 3] = e.deltaX;
    Ma[f + 72 >> 3] = e.deltaY;
    Ma[f + 80 >> 3] = e.deltaZ;
    z[f + 88 >> 2] = e.deltaMode;
    ge(d, 9, f, b) && e.preventDefault();
  }, H:c});
};
function mg(a) {
  return ng(() => new Promise(b => setTimeout(b, a)));
}
mg.uc = !0;
var og = {}, qg = () => {
  if (!pg) {
    var a = {USER:"web_user", LOGNAME:"web_user", PATH:"/", PWD:"/", HOME:"/home/web_user", LANG:(globalThis.navigator?.language ?? "C").replace("-", "_") + ".UTF-8", _:ja || "./this.program"}, b;
    for (b in og) {
      void 0 === og[b] ? delete a[b] : a[b] = og[b];
    }
    var c = [];
    for (b in a) {
      c.push(`${b}=${a[b]}`);
    }
    pg = c;
  }
  return pg;
}, pg, rg = a => {
  for (var b of ["keydown", "mousedown", "touchstart"]) {
    for (var c of [document, document.getElementById("canvas")]) {
      c?.addEventListener(b, () => {
        "suspended" === a.state && a.resume();
      }, {once:!0});
    }
  }
}, sg = a => {
  try {
    a();
  } catch (b) {
    p(b);
  }
};
function tg() {
  var a = ug, b = /^(invoke_.*|__asyncjs__.*)$/;
  for (let [c, d] of Object.entries(a)) {
    if ("function" == typeof d) {
      let e = d.uc || b.test(c);
      a[c] = (...f) => {
        var g = vg;
        try {
          return d(...f);
        } finally {
          f = g === wg && vg === xg;
          var l = c.startsWith("invoke_") && !0;
          vg === g || e || f || l || p(`import ${c} was not in ASYNCIFY_IMPORTS, but changed the state`);
        }
      };
    }
  }
}
function yg(a) {
  var b = (...c) => {
    zg.push(a);
    try {
      return a(...c);
    } finally {
      sa || (c = zg.pop(), k(c === a), Ag && 1 === vg && 0 === zg.length && (vg = wg, sg(Bg), "undefined" != typeof Fibers && Fibers.Cd()));
    }
  };
  Cg.set(a, b);
  return b = Object.defineProperty(b, "name", {value:`__asyncify_wrapper_${a.name}`});
}
var wg = 0, xg = 3, vg = 0, Ag = null, Dg = 0, zg = [], Eg = new Map(), Fg = new Map(), Cg = new Map(), Gg = 0, Hg = null, Ig = [];
function Jg() {
  k(Ag, "Tried to wait for an async operation when none is in progress.");
  k(!Hg, "Cannot have multiple async operations in flight at once");
  return new Promise((a, b) => {
    Hg = {resolve:a, reject:b};
  });
}
function Kg() {
  var a = qd(65548), b = a + 12;
  q[a >> 2] = b;
  q[a + 4 >> 2] = b + 65536;
  b = zg[0];
  k(b, "exportCallStack is empty");
  k(b);
  if (!Eg.has(b)) {
    var c = Gg++;
    Eg.set(b, c);
    Fg.set(c, b);
  }
  b = Eg.get(b);
  z[a + 8 >> 2] = b;
  return a;
}
function Lg() {
  var a = z[Ag + 8 >> 2], b = Fg.get(a);
  k(b, `id ${a} not found in callStackIdToFunc`);
  a = Cg.get(b);
  k(b);
  k(a);
  return yc(a);
}
function Mg(a) {
  k(vg !== xg, "Asyncify cannot be done during or after the runtime exits");
  if (!sa) {
    if (vg === wg) {
      var b = !1, c = !1;
      a((d = 0) => {
        k(["undefined", "number", "boolean", "bigint"].includes(typeof d), `invalid type for handleSleepReturnValue: '${typeof d}'`);
        if (!sa && (Dg = d, b = !0, c)) {
          k(!zg.length, "Waking up (starting to rewind) must be done from JS, without compiled code on the stack.");
          vg = 2;
          sg(() => Ng(Ag));
          "undefined" != typeof zd && td && Kd();
          d = !1;
          try {
            var e = Lg();
          } catch (l) {
            e = l, d = !0;
          }
          var f = !1;
          if (!Ag) {
            var g = Hg;
            g && (Hg = null, (d ? g.reject : g.resolve)(e), f = !0);
          }
          if (d && !f) {
            throw e;
          }
        }
      });
      c = !0;
      b || (vg = 1, Ag = Kg(), "undefined" != typeof zd && td && Jd(), sg(() => Og(Ag)));
    } else {
      2 === vg ? (vg = wg, sg(Pg), Qg(Ag), Ag = null, Ig.forEach(yc)) : p(`invalid state: ${vg}`);
    }
    return Dg;
  }
}
var ng = a => Mg(async b => {
  b(await a());
}), Rg = a => {
  var b = h["_" + a];
  k(b, `Cannot call unknown function ${a}, make sure it is exported`);
  return b;
};
E.bd = (a, b, c, d, e, f, g, l, n, r) => {
  Rb(a, b, c, d, e, l, n, r).then(f).catch(g);
};
E.yd = Rb;
E.Y = Array(4096);
E.m(F, {}, "/");
E.mkdir("/tmp");
E.mkdir("/home");
E.mkdir("/home/web_user");
(function() {
  E.mkdir("/dev");
  zb(E.ga(1, 3), {read:() => 0, write:(d, e, f, g) => g, D:() => 0});
  dc("/dev/null", E.ga(1, 3));
  yb(E.ga(5, 0), Bb);
  yb(E.ga(6, 0), Cb);
  dc("/dev/tty", E.ga(5, 0));
  dc("/dev/tty1", E.ga(6, 0));
  var a = new Uint8Array(1024), b = 0, c = () => {
    0 === b && (qb(a), b = a.byteLength);
    return a[--b];
  };
  E.$("/dev", "random", c);
  E.$("/dev", "urandom", c);
  E.mkdir("/dev/shm");
  E.mkdir("/dev/shm/tmp");
})();
(function() {
  E.mkdir("/proc");
  var a = E.mkdir("/proc/self");
  E.mkdir("/proc/self/fd");
  E.m({m() {
    var b = E.createNode(a, "fd", 16895, 73);
    b.j = {D:F.j.D};
    b.h = {lookup(c, d) {
      c = +d;
      var e = Yb(c);
      c = {parent:null, m:{ua:"fake"}, h:{readlink:() => e.path}, id:c + 1};
      return c.parent = c;
    }, readdir() {
      return Array.from(E.streams.entries()).filter(([, c]) => c).map(([c]) => c.toString());
    }};
    return b;
  }}, {}, "/proc/self/fd");
})();
E.fc = {MEMFS:F, IDBFS:I};
var Sg = () => {
  if (V) {
    var a = V.ra;
    V.ra = V.ya;
    V.ya = a;
    a = V.da;
    V.da = V.Ma;
    V.Ma = a;
    for (a = 0; 21 >= a; ++a) {
      V.da[a] = 0;
    }
  }
};
"undefined" != typeof zd && Fd.push(Sg);
h.requestAnimationFrame = Md;
h.pauseMainLoop = Jd;
h.resumeMainLoop = Kd;
h.preMainLoop && Fd.push(h.preMainLoop);
h.postMainLoop && Gd.push(h.postMainLoop);
for (let a = 0; 32 > a; ++a) {
  Qe.push(Array(a));
}
for (var Tg = new Float32Array(288), Ug = 0; 288 >= Ug; ++Ug) {
  Qf[Ug] = Tg.subarray(0, Ug);
}
var Vg = new Int32Array(288);
for (Ug = 0; 288 >= Ug; ++Ug) {
  Sf[Ug] = Vg.subarray(0, Ug);
}
h.noExitRuntime && (eb = h.noExitRuntime);
h.preloadPlugins && (Mb = h.preloadPlugins);
h.print && (qa = h.print);
h.printErr && (m = h.printErr);
h.wasmBinary && (ra = h.wasmBinary);
Ba("fetchSettings");
Ba("logReadFiles");
Ba("loadSplitModule");
Ba("onMalloc");
Ba("onRealloc");
Ba("onFree");
Ba("onSbrkGrow");
h.arguments && (ia = h.arguments);
h.thisProgram && (ja = h.thisProgram);
k("undefined" == typeof h.memoryInitializerPrefixURL, "Module.memoryInitializerPrefixURL option was removed, use Module.locateFile instead");
k("undefined" == typeof h.pthreadMainPrefixURL, "Module.pthreadMainPrefixURL option was removed, use Module.locateFile instead");
k("undefined" == typeof h.cdInitializerPrefixURL, "Module.cdInitializerPrefixURL option was removed, use Module.locateFile instead");
k("undefined" == typeof h.filePackagePrefixURL, "Module.filePackagePrefixURL option was removed, use Module.locateFile instead");
k("undefined" == typeof h.read, "Module.read option was removed");
k("undefined" == typeof h.readAsync, "Module.readAsync option was removed (modify readAsync in JS)");
k("undefined" == typeof h.readBinary, "Module.readBinary option was removed (modify readBinary in JS)");
k("undefined" == typeof h.setWindowTitle, "Module.setWindowTitle option was removed (modify emscripten_set_window_title in JS)");
k("undefined" == typeof h.TOTAL_MEMORY, "Module.TOTAL_MEMORY has been renamed Module.INITIAL_MEMORY");
k("undefined" == typeof h.ENVIRONMENT, "Module.ENVIRONMENT has been deprecated. To force the environment, use the ENVIRONMENT compile-time option (for example, -sENVIRONMENT=web or -sENVIRONMENT=node)");
k("undefined" == typeof h.STACK_SIZE, "STACK_SIZE can no longer be set at runtime.  Use -sSTACK_SIZE at link time");
k("undefined" == typeof h.wasmMemory, "Use of `wasmMemory` detected.  Use -sIMPORTED_MEMORY to define wasmMemory externally");
k("undefined" == typeof h.INITIAL_MEMORY, "Detected runtime INITIAL_MEMORY setting.  Use -sIMPORTED_MEMORY to define wasmMemory dynamically");
if (h.preInit) {
  for ("function" == typeof h.preInit && (h.preInit = [h.preInit]); 0 < h.preInit.length;) {
    h.preInit.shift()();
  }
}
Aa("preInit");
h.addRunDependency = cb;
h.removeRunDependency = bb;
h.ccall = (a, b, c, d, e) => {
  function f(u) {
    k(0 < uc);
    --uc;
    0 !== r && be(r);
    return "string" === b ? D(u) : "boolean" === b ? !!u : u;
  }
  var g = {string:u => {
    var H = 0;
    null !== u && void 0 !== u && 0 !== u && (H = ae(u));
    return H;
  }, array:u => {
    var H = $d(u.length);
    k(0 <= u.length, "writeArrayToMemory array must have a length (should be an array or typed array)");
    x.set(u, H);
    return H;
  }}, l = Rg(a), n = [], r = 0;
  k("array" !== b, 'Return type should not be "array".');
  if (d) {
    for (var t = 0; t < d.length; t++) {
      var v = g[c[t]];
      v ? (0 === r && (r = sc()), n[t] = v(d[t])) : n[t] = d[t];
    }
  }
  c = Ag;
  l = l(...n);
  e = e?.async;
  uc += 1;
  if (Ag != c) {
    return k(!(c && Ag), "We cannot start an async operation when one is already in flight"), k(!(c && !Ag), "We cannot stop an async operation in flight"), k(e, `The call to ${a} is running asynchronously. If this was intended, add the async option to the ccall/cwrap call.`), Jg().then(f);
  }
  l = f(l);
  return e ? Promise.resolve(l) : l;
};
h.UTF8ToString = D;
h.createContext = Gc;
h.FS_preloadFile = Rb;
h.FS_unlink = (...a) => E.unlink(...a);
h.FS_createPath = (...a) => hc(...a);
h.FS_createDevice = (...a) => E.$(...a);
h.FS = E;
h.FS_createDataFile = (...a) => Qb(...a);
h.FS_createLazyFile = (...a) => kc(...a);
"writeI53ToI64Clamped writeI53ToI64Signaling writeI53ToU64Clamped writeI53ToU64Signaling convertI32PairToI53 convertI32PairToI53Checked convertU32PairToI53 getTempRet0 setTempRet0 zeroMemory withStackSave inetPton4 inetNtop4 inetPton6 inetNtop6 readSockaddr writeSockaddr getDynCaller asmjsMangle HandleAllocator addOnInit addOnPostCtor addOnPreMain STACK_SIZE STACK_ALIGN POINTER_SIZE ASSERTIONS cwrap convertJsFunctionToWasm getEmptyTableSlot updateTableMap getFunctionAddress addFunction removeFunction intArrayToString AsciiToString stringToAscii UTF16ToString stringToUTF16 lengthBytesUTF16 UTF32ToString stringToUTF32 lengthBytesUTF32 fillDeviceOrientationEventData registerDeviceOrientationEventCallback fillDeviceMotionEventData registerDeviceMotionEventCallback screenOrientation fillOrientationChangeEventData registerOrientationChangeEventCallback hideEverythingExceptGivenElement restoreHiddenElements softFullscreenResizeWebGLRenderTarget registerPointerlockErrorEventCallback fillBatteryEventData registerBatteryEventCallback jsStackTrace getCallstack convertPCtoSourceLocation wasiRightsToMuslOFlags wasiOFlagsToMuslOFlags setImmediateWrapped safeRequestAnimationFrame clearImmediateWrapped registerPostMainLoop getPromise makePromise idsToPromises makePromiseCallback ExceptionInfo findMatchingCatch incrementUncaughtExceptionCount decrementUncaughtExceptionCount Browser_asyncPrepareDataCounter arraySum addDays getSocketFromFD getSocketAddress FS_mkdirTree _setNetworkCallback writeGLArray registerWebGlEventCallback ALLOC_NORMAL ALLOC_STACK allocate writeStringToMemory writeAsciiToMemory allocateUTF8 allocateUTF8OnStack demangle stackTrace getNativeTypeSize".split(" ").forEach(function(a) {
  Da(a, () => {
    var b = `\`${a}\` is a library symbol and not included by default; add it to your library.js __deps or to DEFAULT_LIBRARY_FUNCS_TO_INCLUDE on the command line`, c = a;
    c.startsWith("_") || (c = "$" + a);
    b += ` (e.g. -sDEFAULT_LIBRARY_FUNCS_TO_INCLUDE='${c}')`;
    Ca(a) && (b += ". Alternatively, forcing filesystem support (-sFORCE_FILESYSTEM) can export this for you");
    Fa(b);
  });
  Ga(a);
});
"run out err callMain abort wasmExports writeStackCookie checkStackCookie writeI53ToI64 readI53FromI64 readI53FromU64 INT53_MAX INT53_MIN bigintToI53Checked HEAP8 HEAPU8 HEAP16 HEAPU16 HEAP32 HEAPU32 HEAPF32 HEAPF64 HEAP64 HEAPU64 stackSave stackRestore stackAlloc createNamedFunction ptrToString exitJS getHeapMax growMemory ENV ERRNO_CODES strError DNS Protocols Sockets timers warnOnce readEmAsmArgsArray readEmAsmArgs runEmAsmFunction runMainThreadEmAsm jstoi_q getExecutableName autoResumeAudioContext dynCallLegacy dynCall handleException keepRuntimeAlive runtimeKeepalivePush runtimeKeepalivePop callUserCallback maybeExit asyncLoad alignMemory mmapAlloc wasmTable wasmMemory getUniqueRunDependency noExitRuntime addOnPreRun addOnExit addOnPostRun freeTableIndexes functionsInTableMap setValue getValue PATH PATH_FS UTF8Decoder UTF8ArrayToString stringToUTF8Array stringToUTF8 lengthBytesUTF8 intArrayFromString UTF16Decoder stringToNewUTF8 stringToUTF8OnStack writeArrayToMemory JSEvents registerKeyEventCallback specialHTMLTargets maybeCStringToJsString findEventTarget findCanvasEventTarget getBoundingClientRect fillMouseEventData registerMouseEventCallback registerWheelEventCallback registerUiEventCallback registerFocusEventCallback fillFullscreenChangeEventData registerFullscreenChangeEventCallback JSEvents_requestFullscreen JSEvents_resizeCanvasForFullscreen registerRestoreOldStyle setLetterbox currentFullscreenStrategy restoreOldWindowedStyle doRequestFullscreen fillPointerlockChangeEventData registerPointerlockChangeEventCallback requestPointerLock fillVisibilityChangeEventData registerVisibilityChangeEventCallback registerTouchEventCallback fillGamepadEventData registerGamepadEventCallback registerBeforeUnloadEventCallback setCanvasElementSize getCanvasElementSize UNWIND_CACHE ExitStatus getEnvStrings checkWasiClock doReadv doWritev initRandomFill randomFill safeSetTimeout emSetImmediate emClearImmediate_deps emClearImmediate registerPreMainLoop promiseMap uncaughtExceptionCount exceptionCaught Browser requestFullscreen requestFullScreen setCanvasSize getUserMedia getPreloadedImageData__data wget MONTH_DAYS_REGULAR MONTH_DAYS_LEAP MONTH_DAYS_REGULAR_CUMULATIVE MONTH_DAYS_LEAP_CUMULATIVE isLeapYear ydayFromDate SYSCALLS preloadPlugins FS_createPreloadedFile FS_modeStringToFlags FS_getMode FS_fileDataToTypedArray FS_stdin_getChar_buffer FS_stdin_getChar FS_readFile FS_root FS_mounts FS_devices FS_streams FS_nextInode FS_nameTable FS_currentPath FS_initialized FS_ignorePermissions FS_filesystems FS_syncFSRequests FS_lookupPath FS_getPath FS_hashName FS_hashAddNode FS_hashRemoveNode FS_lookupNode FS_createNode FS_destroyNode FS_isRoot FS_isMountpoint FS_isFile FS_isDir FS_isLink FS_isChrdev FS_isBlkdev FS_isFIFO FS_isSocket FS_flagsToPermissionString FS_nodePermissions FS_mayLookup FS_mayCreate FS_mayDelete FS_mayOpen FS_checkOpExists FS_nextfd FS_getStreamChecked FS_getStream FS_createStream FS_closeStream FS_dupStream FS_doSetAttr FS_chrdev_stream_ops FS_major FS_minor FS_makedev FS_registerDevice FS_getDevice FS_getMounts FS_syncfs FS_mount FS_unmount FS_lookup FS_mknod FS_statfs FS_statfsStream FS_statfsNode FS_create FS_mkdir FS_mkdev FS_symlink FS_rename FS_rmdir FS_readdir FS_readlink FS_stat FS_fstat FS_lstat FS_doChmod FS_chmod FS_lchmod FS_fchmod FS_doChown FS_chown FS_lchown FS_fchown FS_doTruncate FS_truncate FS_ftruncate FS_utime FS_open FS_close FS_isClosed FS_llseek FS_read FS_write FS_mmap FS_msync FS_ioctl FS_writeFile FS_cwd FS_chdir FS_createDefaultDirectories FS_createDefaultDevices FS_createSpecialDirectories FS_createStandardStreams FS_staticInit FS_init FS_quit FS_findObject FS_analyzePath FS_createFile FS_forceLoadFile MEMFS TTY PIPEFS SOCKFS tempFixedLengthArray miniTempWebGLFloatBuffers miniTempWebGLIntBuffers heapObjectForWebGLType toTypedArrayIndex webgl_enable_ANGLE_instanced_arrays webgl_enable_OES_vertex_array_object webgl_enable_WEBGL_draw_buffers webgl_enable_WEBGL_multi_draw webgl_enable_EXT_polygon_offset_clamp webgl_enable_EXT_clip_control webgl_enable_WEBGL_polygon_mode GL emscriptenWebGLGet computeUnpackAlignedImageSize colorChannelsInGlTextureFormat emscriptenWebGLGetTexPixelData emscriptenWebGLGetUniform webglGetUniformLocation webglPrepareUniformLocationsBeforeFirstUse webglGetLeftBracePos emscriptenWebGLGetVertexAttrib __glGetActiveAttribOrUniform emscriptenWebGLGetBufferBinding emscriptenWebGLValidateMapBufferTarget AL GLUT EGL GLEW IDBStore runAndAbortIfError Asyncify Fibers emscriptenWebGLGetIndexed webgl_enable_WEBGL_draw_instanced_base_vertex_base_instance webgl_enable_WEBGL_multi_draw_instanced_base_vertex_base_instance print printErr jstoi_s IDBFS".split(" ").forEach(Ga);
var Qd = {155644:() => h.canvas.width, 155676:() => h.canvas.height, 155709:() => {
  window.print();
}, 155724:a => {
  a = D(a);
  var b = a.split("/").pop();
  try {
    var c = E.readFile(a), d = "application/octet-stream";
    b.endsWith(".svg") ? d = "image/svg+xml" : b.endsWith(".png") ? d = "image/png" : b.endsWith(".jpg") || b.endsWith(".jpeg") ? d = "image/jpeg" : b.endsWith(".csv") && (d = "text/csv");
    var e = URL.createObjectURL(new Blob([c], {type:d})), f = document.createElement("a");
    f.href = e;
    f.download = b;
    document.body.appendChild(f);
    f.click();
    document.body.removeChild(f);
    URL.revokeObjectURL(e);
  } catch (g) {
    console.error("[Aslan] Download failed for", a, ":", g);
  }
}, 156449:a => {
  function b() {
    c.parentNode && c.parentNode.removeChild(c);
  }
  a = D(a);
  var c = document.createElement("input");
  c.type = "file";
  c.accept = a;
  c.style.display = "none";
  document.body.appendChild(c);
  c.onchange = function(d) {
    var e = d.target.files[0];
    b();
    e ? (d = new FileReader(), d.onload = function(f) {
      f = new Uint8Array(f.target.result);
      try {
        E.mkdir("/tmp");
      } catch (l) {
      }
      var g = "/tmp/" + e.name;
      E.writeFile(g, f);
      h.Oa("web_on_file_ready", null, ["string"], [g]);
    }, d.onerror = function() {
      b();
      h.Oa("web_on_file_cancelled", null, [], []);
    }, d.readAsArrayBuffer(e)) : h.Oa("web_on_file_cancelled", null, [], []);
  };
  c.addEventListener("cancel", function() {
    b();
    h.Oa("web_on_file_cancelled", null, [], []);
  });
  c.click();
}, 157382:() => "undefined" !== typeof AudioContext || "undefined" !== typeof webkitAudioContext ? !0 : !1, 157529:() => "undefined" !== typeof navigator.mediaDevices && "undefined" !== typeof navigator.mediaDevices.getUserMedia || "undefined" !== typeof navigator.webkitGetUserMedia ? !0 : !1, 157763:a => {
  "undefined" === typeof h.SDL2 && (h.SDL2 = {});
  var b = h.SDL2;
  a ? b.capture = {} : b.audio = {};
  b.o || ("undefined" !== typeof AudioContext ? b.o = new AudioContext() : "undefined" !== typeof webkitAudioContext && (b.o = new webkitAudioContext()), b.o && "undefined" === typeof navigator.userActivation && rg(b.o));
  return void 0 === b.o ? -1 : 0;
}, 158315:() => h.SDL2.o.sampleRate, 158383:(a, b, c, d) => {
  function e() {
  }
  function f(l) {
    void 0 !== g.capture.S && (clearInterval(g.capture.S), g.capture.S = void 0, g.capture.ia = void 0);
    g.capture.cb = g.o.createMediaStreamSource(l);
    g.capture.P = g.o.createScriptProcessor(b, a, 1);
    g.capture.P.onaudioprocess = function(n) {
      void 0 !== g && void 0 !== g.capture && (n.outputBuffer.getChannelData(0).fill(0.0), g.capture.Pa = n.inputBuffer, db(c, [d]));
    };
    g.capture.cb.connect(g.capture.P);
    g.capture.P.connect(g.o.destination);
    g.capture.stream = l;
  }
  var g = h.SDL2;
  g.capture.ia = g.o.createBuffer(a, b, g.o.sampleRate);
  g.capture.ia.getChannelData(0).fill(0.0);
  g.capture.S = setInterval(function() {
    g.capture.Pa = g.capture.ia;
    db(c, [d]);
  }, b / g.o.sampleRate * 1000);
  void 0 !== navigator.mediaDevices && void 0 !== navigator.mediaDevices.getUserMedia ? navigator.mediaDevices.getUserMedia({audio:!0, video:!1}).then(f).catch(e) : void 0 !== navigator.webkitGetUserMedia && navigator.webkitGetUserMedia({audio:!0, video:!1}, f, e);
}, 160076:(a, b, c, d) => {
  var e = h.SDL2;
  e.audio.P = e.o.createScriptProcessor(b, 0, a);
  e.audio.P.onaudioprocess = function(f) {
    void 0 !== e && void 0 !== e.audio && (void 0 !== e.audio.S && (clearInterval(e.audio.S), e.audio.S = void 0, e.audio.ia = void 0), e.audio.Da = f.outputBuffer, db(c, [d]));
  };
  e.audio.P.connect(e.o.destination);
  "suspended" === e.o.state && (e.audio.ia = e.o.createBuffer(a, b, e.o.sampleRate), e.audio.ia.getChannelData(0).fill(0.0), e.audio.S = setInterval(function() {
    "undefined" !== typeof navigator.userActivation && navigator.userActivation.hasBeenActive && e.o.resume();
    e.audio.Da = e.audio.ia;
    db(c, [d]);
    e.audio.Da = void 0;
  }, b / e.o.sampleRate * 1000));
}, 161251:(a, b) => {
  for (var c = h.SDL2, d = c.capture.Pa.numberOfChannels, e = 0; e < d; ++e) {
    var f = c.capture.Pa.getChannelData(e);
    if (f.length != b) {
      throw "Web Audio capture buffer length mismatch! Destination size: " + f.length + " samples vs expected " + b + " samples!";
    }
    if (1 == d) {
      for (var g = 0; g < b; ++g) {
        fb(a + 4 * g, f[g]);
      }
    } else {
      for (g = 0; g < b; ++g) {
        fb(a + 4 * (g * d + e), f[g]);
      }
    }
  }
}, 161856:(a, b) => {
  var c = h.SDL2;
  a >>>= 2;
  for (var d = c.audio.Da.numberOfChannels, e = 0; e < d; ++e) {
    var f = c.audio.Da.getChannelData(e);
    if (f.length != b) {
      throw "Web Audio output buffer length mismatch! Destination size: " + f.length + " samples vs expected " + b + " samples!";
    }
    for (var g = 0; g < b; ++g) {
      f[g] = A[a + (g * d + e)];
    }
  }
}, 162345:a => {
  var b = h.SDL2;
  if (a) {
    void 0 !== b.capture.S && clearInterval(b.capture.S);
    if (void 0 !== b.capture.stream) {
      a = b.capture.stream.getAudioTracks();
      for (var c = 0; c < a.length; c++) {
        b.capture.stream.removeTrack(a[c]);
      }
    }
    void 0 !== b.capture.P && (b.capture.P.onaudioprocess = function() {
    }, b.capture.P.disconnect());
    void 0 !== b.capture.cb && b.capture.cb.disconnect();
    b.capture = void 0;
  } else {
    void 0 != b.audio.P && b.audio.P.disconnect(), void 0 !== b.audio.S && clearInterval(b.audio.S), b.audio = void 0;
  }
  void 0 !== b.o && void 0 === b.audio && void 0 === b.capture && (b.o.close(), b.o = void 0);
}, 163351:(a, b, c) => {
  h.SDL2 || (h.SDL2 = {});
  var d = h.SDL2;
  d.Ub !== h.canvas && (d.Ca = Gc(h.canvas, !1, !0), d.Ub = h.canvas);
  if (d.w !== a || d.nc !== b || d.pc !== d.Ca) {
    d.image = d.Ca.createImageData(a, b), d.w = a, d.nc = b, d.pc = d.Ca;
  }
  a = d.image.data;
  b = c / 4;
  var e = 0;
  if ("undefined" !== typeof CanvasPixelArray && a instanceof CanvasPixelArray) {
    for (c = a.length; e < c;) {
      var f = z[b];
      a[e] = f & 255;
      a[e + 1] = f >> 8 & 255;
      a[e + 2] = f >> 16 & 255;
      a[e + 3] = 255;
      b++;
      e += 4;
    }
  } else {
    if (d.Yb !== a && (d.Xb = new Int32Array(a.buffer), d.Zb = new Uint8Array(a.buffer), d.Yb = a), a = d.Xb, c = a.length, a.set(z.subarray(b, b + c)), a = d.Zb, b = 3, e = b + 4 * c, 0 == c % 8) {
      for (; b < e;) {
        a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0;
      }
    } else {
      for (; b < e;) {
        a[b] = 255, b = b + 4 | 0;
      }
    }
  }
  d.Ca.putImageData(d.image, 0, 0);
}, 164817:(a, b, c, d, e) => {
  var f = document.createElement("canvas");
  f.width = a;
  f.height = b;
  var g = f.getContext("2d");
  a = g.createImageData(a, b);
  b = a.data;
  e /= 4;
  var l = 0, n;
  if ("undefined" !== typeof CanvasPixelArray && b instanceof CanvasPixelArray) {
    for (n = b.length; l < n;) {
      var r = z[e];
      b[l] = r & 255;
      b[l + 1] = r >> 8 & 255;
      b[l + 2] = r >> 16 & 255;
      b[l + 3] = r >> 24 & 255;
      e++;
      l += 4;
    }
  } else {
    b = new Int32Array(b.buffer), n = b.length, b.set(z.subarray(e, e + n));
  }
  g.putImageData(a, 0, 0);
  c = 0 === c && 0 === d ? "url(" + f.toDataURL() + "), auto" : "url(" + f.toDataURL() + ") " + c + " " + d + ", auto";
  d = qd(c.length + 1);
  K(c, d, c.length + 1);
  return d;
}, 165805:a => {
  h.canvas && (h.canvas.style.cursor = D(a));
}, 165888:() => {
  h.canvas && (h.canvas.style.cursor = "none");
}, 165957:() => window.innerWidth, 165987:() => window.innerHeight}, Wg = h._main = w("_main"), Qg = w("_free"), qd = w("_malloc"), mc = w("_fflush");
h._web_on_file_ready = w("_web_on_file_ready");
h._web_on_file_cancelled = w("_web_on_file_cancelled");
var lc = w("_strerror"), va = w("_emscripten_stack_get_end"), Xg = w("_emscripten_stack_init"), be = w("__emscripten_stack_restore"), $d = w("__emscripten_stack_alloc"), sc = w("_emscripten_stack_get_current"), dynCall_v = w("dynCall_v"), ge = w("dynCall_iiii"), dynCall_vii = w("dynCall_vii"), dynCall_vi = w("dynCall_vi"), dynCall_iii = w("dynCall_iii"), Og = w("_asyncify_start_unwind"), Bg = w("_asyncify_stop_unwind"), Ng = w("_asyncify_start_rewind"), Pg = w("_asyncify_stop_rewind"), Ja = w("wasmMemory"), 
ug = {__assert_fail:(a, b, c, d) => p(`Assertion failed: ${D(a)}, at: ` + [b ? D(b) : "unknown filename", c, d ? D(d) : "unknown function"]), __syscall_fcntl64:function(a, b, c) {
  ib = c;
  try {
    var d = Yb(a);
    switch(b) {
      case 0:
        var e = jb();
        if (0 > e) {
          break;
        }
        for (; E.streams[e];) {
          e++;
        }
        return $b(d, e).fd;
      case 1:
      case 2:
        return 0;
      case 3:
        return d.flags;
      case 4:
        return e = jb(), d.flags |= e, 0;
      case 12:
        return e = jb(), Ka[e + 0 >> 1] = 2, 0;
      case 13:
      case 14:
        return 0;
    }
    return -28;
  } catch (f) {
    if ("undefined" == typeof E || "ErrnoError" !== f.name) {
      throw f;
    }
    return -f.s;
  }
}, __syscall_fstat64:function(a, b) {
  try {
    return oc(b, E.fstat(a));
  } catch (c) {
    if ("undefined" == typeof E || "ErrnoError" !== c.name) {
      throw c;
    }
    return -c.s;
  }
}, __syscall_ioctl:function(a, b, c) {
  ib = c;
  try {
    var d = Yb(a);
    switch(b) {
      case 21509:
        return d.tty ? 0 : -59;
      case 21505:
        if (!d.tty) {
          return -59;
        }
        if (d.tty.Z.rc) {
          a = [3, 28, 127, 21, 4, 0, 1, 0, 17, 19, 26, 0, 18, 15, 23, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
          var e = jb();
          z[e >> 2] = 25856;
          z[e + 4 >> 2] = 5;
          z[e + 8 >> 2] = 191;
          z[e + 12 >> 2] = 35387;
          for (var f = 0; 32 > f; f++) {
            x[e + f + 17] = a[f] || 0;
          }
        }
        return 0;
      case 21510:
      case 21511:
      case 21512:
        return d.tty ? 0 : -59;
      case 21506:
      case 21507:
      case 21508:
        if (!d.tty) {
          return -59;
        }
        if (d.tty.Z.sc) {
          for (e = jb(), a = [], f = 0; 32 > f; f++) {
            a.push(x[e + f + 17]);
          }
        }
        return 0;
      case 21519:
        if (!d.tty) {
          return -59;
        }
        e = jb();
        return z[e >> 2] = 0;
      case 21520:
        return d.tty ? -28 : -59;
      case 21537:
      case 21531:
        return e = jb(), E.Wa(d, b, e);
      case 21523:
        if (!d.tty) {
          return -59;
        }
        d.tty.Z.tc && (f = [24, 80], e = jb(), Ka[e >> 1] = f[0], Ka[e + 2 >> 1] = f[1]);
        return 0;
      case 21524:
        return d.tty ? 0 : -59;
      case 21515:
        return d.tty ? 0 : -59;
      default:
        return -28;
    }
  } catch (g) {
    if ("undefined" == typeof E || "ErrnoError" !== g.name) {
      throw g;
    }
    return -g.s;
  }
}, __syscall_lstat64:function(a, b) {
  try {
    return a = D(a), oc(b, E.lstat(a));
  } catch (c) {
    if ("undefined" == typeof E || "ErrnoError" !== c.name) {
      throw c;
    }
    return -c.s;
  }
}, __syscall_mkdirat:function(a, b, c) {
  try {
    return b = D(b), b = nc(a, b), E.mkdir(b, c, 0), 0;
  } catch (d) {
    if ("undefined" == typeof E || "ErrnoError" !== d.name) {
      throw d;
    }
    return -d.s;
  }
}, __syscall_newfstatat:function(a, b, c, d) {
  try {
    b = D(b);
    var e = d & 256, f = d & 4096;
    d &= -6401;
    k(!d, `unknown flags in __syscall_newfstatat: ${d}`);
    b = nc(a, b, f);
    return oc(c, e ? E.lstat(b) : E.stat(b));
  } catch (g) {
    if ("undefined" == typeof E || "ErrnoError" !== g.name) {
      throw g;
    }
    return -g.s;
  }
}, __syscall_openat:function(a, b, c, d) {
  ib = d;
  try {
    b = D(b);
    b = nc(a, b);
    var e = d ? jb() : 0;
    return E.open(b, c, e).fd;
  } catch (f) {
    if ("undefined" == typeof E || "ErrnoError" !== f.name) {
      throw f;
    }
    return -f.s;
  }
}, __syscall_stat64:function(a, b) {
  try {
    return a = D(a), oc(b, E.stat(a));
  } catch (c) {
    if ("undefined" == typeof E || "ErrnoError" !== c.name) {
      throw c;
    }
    return -c.s;
  }
}, _abort_js:() => p("native code called abort()"), _localtime_js:function(a, b) {
  a = -9007199254740992 > a || 9007199254740992 < a ? NaN : Number(a);
  a = new Date(1000 * a);
  z[b >> 2] = a.getSeconds();
  z[b + 4 >> 2] = a.getMinutes();
  z[b + 8 >> 2] = a.getHours();
  z[b + 12 >> 2] = a.getDate();
  z[b + 16 >> 2] = a.getMonth();
  z[b + 20 >> 2] = a.getFullYear() - 1900;
  z[b + 24 >> 2] = a.getDay();
  z[b + 28 >> 2] = (pc(a.getFullYear()) ? qc : rc)[a.getMonth()] + a.getDate() - 1 | 0;
  z[b + 36 >> 2] = -(60 * a.getTimezoneOffset());
  var c = (new Date(a.getFullYear(), 6, 1)).getTimezoneOffset(), d = (new Date(a.getFullYear(), 0, 1)).getTimezoneOffset();
  z[b + 32 >> 2] = (c != d && a.getTimezoneOffset() == Math.min(d, c)) | 0;
}, _mktime_js:function(a) {
  var b = new Date(z[a + 20 >> 2] + 1900, z[a + 16 >> 2], z[a + 12 >> 2], z[a + 8 >> 2], z[a + 4 >> 2], z[a >> 2], 0), c = z[a + 32 >> 2], d = b.getTimezoneOffset(), e = (new Date(b.getFullYear(), 6, 1)).getTimezoneOffset(), f = (new Date(b.getFullYear(), 0, 1)).getTimezoneOffset(), g = Math.min(f, e);
  0 > c ? z[a + 32 >> 2] = Number(e != f && g == d) : 0 < c != (g == d) && (e = Math.max(f, e), b.setTime(b.getTime() + 60000 * ((0 < c ? g : e) - d)));
  z[a + 24 >> 2] = b.getDay();
  z[a + 28 >> 2] = (pc(b.getFullYear()) ? qc : rc)[b.getMonth()] + b.getDate() - 1 | 0;
  z[a >> 2] = b.getSeconds();
  z[a + 4 >> 2] = b.getMinutes();
  z[a + 8 >> 2] = b.getHours();
  z[a + 12 >> 2] = b.getDate();
  z[a + 16 >> 2] = b.getMonth();
  z[a + 20 >> 2] = b.getYear();
  a = b.getTime();
  return BigInt(isNaN(a) ? -1 : a / 1000);
}, _tzset_js:(a, b, c, d) => {
  var e = (new Date()).getFullYear(), f = (new Date(e, 0, 1)).getTimezoneOffset();
  e = (new Date(e, 6, 1)).getTimezoneOffset();
  q[a >> 2] = 60 * Math.max(f, e);
  z[b >> 2] = Number(f != e);
  b = g => {
    var l = Math.abs(g);
    return `UTC${0 <= g ? "-" : "+"}${String(Math.floor(l / 60)).padStart(2, "0")}${String(l % 60).padStart(2, "0")}`;
  };
  a = b(f);
  b = b(e);
  k(a);
  k(b);
  k(16 >= ub(a), `timezone name truncated to fit in TZNAME_MAX (${a})`);
  k(16 >= ub(b), `timezone name truncated to fit in TZNAME_MAX (${b})`);
  e < f ? (K(a, c, 17), K(b, d, 17)) : (K(a, d, 17), K(b, c, 17));
}, clock_time_get:function(a, b, c) {
  if (!(0 <= a && 3 >= a)) {
    return 28;
  }
  Na[c >> 3] = BigInt(Math.round(1E6 * (0 === a ? Date.now() : performance.now())));
  return 0;
}, eglBindAPI:a => {
  if (12448 == a) {
    return M = 12288, 1;
  }
  M = 12300;
  return 0;
}, eglChooseConfig:(a, b, c, d, e) => {
  if (62000 != a) {
    M = 12296, c = 0;
  } else {
    if (b) {
      for (;;) {
        a = z[b >> 2];
        if (12321 == a) {
          O.alpha = 0 < z[b + 4 >> 2];
        } else if (12325 == a) {
          O.depth = 0 < z[b + 4 >> 2];
        } else if (12326 == a) {
          O.stencil = 0 < z[b + 4 >> 2];
        } else if (12337 == a) {
          a = z[b + 4 >> 2], O.antialias = 0 < a;
        } else if (12338 == a) {
          a = z[b + 4 >> 2], O.antialias = 1 == a;
        } else if (12544 == a) {
          O.rd = 12547 != z[b + 4 >> 2];
        } else if (12344 == a) {
          break;
        }
        b += 8;
      }
    }
    c && d || e ? (e && (z[e >> 2] = 1), c && 0 < d && (q[c >> 2] = 62002), M = 12288, c = 1) : (M = 12300, c = 0);
  }
  return c;
}, eglCreateContext:(a, b, c, d) => {
  if (62000 != a) {
    return M = 12296, 0;
  }
  for (a = 1;;) {
    b = z[d >> 2];
    if (12440 == b) {
      a = z[d + 4 >> 2];
    } else if (12344 == b) {
      break;
    } else {
      return M = 12292, 0;
    }
    d += 8;
  }
  if (2 > a || 3 < a) {
    return M = 12293, 0;
  }
  O.ab = a - 1;
  O.ud = 0;
  Qc = Ic(h.canvas, O);
  if (0 != Qc) {
    return M = 12288, Kc(Qc), Dc.forEach(e => e()), Kc(null), 62004;
  }
  M = 12297;
  return 0;
}, eglCreateWindowSurface:(a, b) => {
  if (62000 != a) {
    return M = 12296, 0;
  }
  if (62002 != b) {
    return M = 12293, 0;
  }
  M = 12288;
  return 62006;
}, eglDestroyContext:(a, b) => {
  if (62000 != a) {
    return M = 12296, 0;
  }
  if (62004 != b) {
    return M = 12294, 0;
  }
  a = Qc;
  V === Jc[a] && (V = null);
  "object" == typeof X && X.Gc(Jc[a].V.canvas);
  Jc[a]?.V.canvas && (Jc[a].V.canvas.sa = void 0);
  Jc[a] = null;
  M = 12288;
  Mc == b && (Mc = 0);
  return 1;
}, eglDestroySurface:(a, b) => {
  if (62000 != a) {
    return M = 12296, 0;
  }
  if (62006 != b) {
    return M = 12301, 1;
  }
  Nc == b && (Nc = 0);
  Oc == b && (Oc = 0);
  M = 12288;
  return 1;
}, eglGetConfigAttrib:(a, b, c, d) => {
  if (62000 != a) {
    return M = 12296, 0;
  }
  if (62002 != b) {
    return M = 12293, 0;
  }
  if (!d) {
    return M = 12300, 0;
  }
  M = 12288;
  switch(c) {
    case 12320:
      return z[d >> 2] = O.alpha ? 32 : 24, 1;
    case 12321:
      return z[d >> 2] = O.alpha ? 8 : 0, 1;
    case 12322:
      return z[d >> 2] = 8, 1;
    case 12323:
      return z[d >> 2] = 8, 1;
    case 12324:
      return z[d >> 2] = 8, 1;
    case 12325:
      return z[d >> 2] = O.depth ? 24 : 0, 1;
    case 12326:
      return z[d >> 2] = O.stencil ? 8 : 0, 1;
    case 12327:
      return z[d >> 2] = 12344, 1;
    case 12328:
      return z[d >> 2] = 62002, 1;
    case 12329:
      return z[d >> 2] = 0, 1;
    case 12330:
      return z[d >> 2] = 4096, 1;
    case 12331:
      return z[d >> 2] = 16777216, 1;
    case 12332:
      return z[d >> 2] = 4096, 1;
    case 12333:
      return z[d >> 2] = 0, 1;
    case 12334:
      return z[d >> 2] = 0, 1;
    case 12335:
      return z[d >> 2] = 12344, 1;
    case 12337:
      return z[d >> 2] = O.antialias ? 4 : 0, 1;
    case 12338:
      return z[d >> 2] = O.antialias ? 1 : 0, 1;
    case 12339:
      return z[d >> 2] = 4, 1;
    case 12340:
      return z[d >> 2] = 12344, 1;
    case 12341:
    case 12342:
    case 12343:
      return z[d >> 2] = -1, 1;
    case 12345:
    case 12346:
      return z[d >> 2] = 0, 1;
    case 12347:
      return z[d >> 2] = 0, 1;
    case 12348:
      return z[d >> 2] = 1;
    case 12349:
    case 12350:
      return z[d >> 2] = 0, 1;
    case 12351:
      return z[d >> 2] = 12430, 1;
    case 12352:
      return z[d >> 2] = 4, 1;
    case 12354:
      return z[d >> 2] = 0, 1;
    default:
      return M = 12292, 0;
  }
}, eglGetDisplay:a => {
  M = 12288;
  return 0 != a && 1 != a ? 0 : 62000;
}, eglGetError:() => M, eglInitialize:(a, b, c) => {
  if (62000 != a) {
    return M = 12296, 0;
  }
  b && (z[b >> 2] = 1);
  c && (z[c >> 2] = 4);
  Lc = !0;
  M = 12288;
  return 1;
}, eglMakeCurrent:(a, b, c, d) => {
  if (62000 != a) {
    return M = 12296, 0;
  }
  if (0 != d && 62004 != d) {
    return M = 12294, 0;
  }
  if (0 != c && 62006 != c || 0 != b && 62006 != b) {
    return M = 12301, 0;
  }
  Kc(d ? Qc : null);
  Mc = d;
  Oc = b;
  Nc = c;
  M = 12288;
  return 1;
}, eglQueryString:(a, b) => {
  if (62000 != a) {
    return M = 12296, 0;
  }
  M = 12288;
  if (Pc[b]) {
    return Pc[b];
  }
  switch(b) {
    case 12371:
      a = rd("Emscripten");
      break;
    case 12372:
      a = rd("1.4 Emscripten EGL");
      break;
    case 12373:
      a = rd("");
      break;
    case 12429:
      a = rd("OpenGL_ES");
      break;
    default:
      return M = 12300, 0;
  }
  return Pc[b] = a;
}, eglSwapBuffers:() => {
  if (Lc) {
    if (L) {
      if (L.isContextLost()) {
        M = 12302;
      } else {
        return M = 12288, 1;
      }
    } else {
      M = 12290;
    }
  } else {
    M = 12289;
  }
  return 0;
}, eglSwapInterval:(a, b) => {
  if (62000 != a) {
    return M = 12296, 0;
  }
  0 == b ? Hd(0, 0) : Hd(1, b);
  M = 12288;
  return 1;
}, eglTerminate:a => {
  if (62000 != a) {
    return M = 12296, 0;
  }
  Oc = Nc = Mc = 0;
  Lc = !1;
  M = 12288;
  return 1;
}, eglWaitGL:() => {
  M = 12288;
  return 1;
}, eglWaitNative:() => {
  M = 12288;
  return 1;
}, emscripten_asm_const_int:(a, b, c) => {
  b = Pd(b, c);
  k(Qd.hasOwnProperty(a), `No EM_ASM constant found at address ${a}.  The loaded WebAssembly file is likely out of sync with the generated JavaScript.`);
  return Qd[a](...b);
}, emscripten_asm_const_int_sync_on_main_thread:(a, b, c) => Rd(a, b, c), emscripten_asm_const_ptr_sync_on_main_thread:(a, b, c) => Rd(a, b, c), emscripten_date_now:() => Date.now(), emscripten_exit_fullscreen:() => {
  if (!X.fullscreenEnabled()) {
    return -1;
  }
  Ud(ke);
  var a = Zd[1];
  if (a.exitFullscreen) {
    a.fullscreenElement && a.exitFullscreen();
  } else if (a.webkitExitFullscreen) {
    a.webkitFullscreenElement && a.webkitExitFullscreen();
  } else {
    return -1;
  }
  return 0;
}, emscripten_exit_pointerlock:() => {
  Ud(le);
  if (!document.exitPointerLock) {
    return -1;
  }
  document.exitPointerLock();
  return 0;
}, emscripten_get_device_pixel_ratio:() => globalThis.devicePixelRatio ?? 1.0, emscripten_get_element_css_size:(a, b, c) => {
  a = Y(a);
  if (!a) {
    return -4;
  }
  a = je(a);
  Ma[b >> 3] = a.width;
  Ma[c >> 3] = a.height;
  return 0;
}, emscripten_get_gamepad_status:(a, b) => {
  k(X.ma, "emscripten_get_gamepad_status() can only be called after having first called emscripten_sample_gamepad_data() and that function has returned EMSCRIPTEN_RESULT_SUCCESS!");
  if (0 > a || a >= X.ma.length) {
    return -5;
  }
  if (!X.ma[a]) {
    return -7;
  }
  me(b, X.ma[a]);
  return 0;
}, emscripten_get_now:() => performance.now(), emscripten_get_num_gamepads:() => {
  k(X.ma, "emscripten_get_num_gamepads() can only be called after having first called emscripten_sample_gamepad_data() and that function has returned EMSCRIPTEN_RESULT_SUCCESS!");
  return X.ma.length;
}, emscripten_get_screen_size:(a, b) => {
  z[a >> 2] = screen.width;
  z[b >> 2] = screen.height;
}, emscripten_glActiveTexture:ne, emscripten_glAttachShader:oe, emscripten_glBeginQuery:(a, b) => {
  L.beginQuery(a, R[b]);
}, emscripten_glBeginQueryEXT:(a, b) => {
  L.J.beginQueryEXT(a, R[b]);
}, emscripten_glBeginTransformFeedback:a => L.beginTransformFeedback(a), emscripten_glBindAttribLocation:pe, emscripten_glBindBuffer:qe, emscripten_glBindBufferBase:(a, b, c) => {
  L.bindBufferBase(a, b, Wc[c]);
}, emscripten_glBindBufferRange:(a, b, c, d, e) => {
  L.bindBufferRange(a, b, Wc[c], d, e);
}, emscripten_glBindFramebuffer:re, emscripten_glBindRenderbuffer:se, emscripten_glBindSampler:(a, b) => {
  L.bindSampler(a, bd[b]);
}, emscripten_glBindTexture:te, emscripten_glBindTransformFeedback:(a, b) => {
  L.bindTransformFeedback(a, cd[b]);
}, emscripten_glBindVertexArray:ue, emscripten_glBindVertexArrayOES:ue, emscripten_glBlendColor:(a, b, c, d) => L.blendColor(a, b, c, d), emscripten_glBlendEquation:a => L.blendEquation(a), emscripten_glBlendEquationSeparate:(a, b) => L.blendEquationSeparate(a, b), emscripten_glBlendFunc:(a, b) => L.blendFunc(a, b), emscripten_glBlendFuncSeparate:ve, emscripten_glBlitFramebuffer:(a, b, c, d, e, f, g, l, n, r) => L.blitFramebuffer(a, b, c, d, e, f, g, l, n, r), emscripten_glBufferData:we, emscripten_glBufferSubData:(a, 
b, c, d) => {
  2 <= V.version ? c && L.bufferSubData(a, b, y, d, c) : L.bufferSubData(a, b, y.subarray(d, d + c));
}, emscripten_glCheckFramebufferStatus:xe, emscripten_glClear:ye, emscripten_glClearBufferfi:(a, b, c, d) => L.clearBufferfi(a, b, c, d), emscripten_glClearBufferfv:(a, b, c) => {
  L.clearBufferfv(a, b, A, c >> 2);
}, emscripten_glClearBufferiv:(a, b, c) => {
  L.clearBufferiv(a, b, z, c >> 2);
}, emscripten_glClearBufferuiv:(a, b, c) => {
  L.clearBufferuiv(a, b, q, c >> 2);
}, emscripten_glClearColor:ze, emscripten_glClearDepthf:a => L.clearDepth(a), emscripten_glClearStencil:a => L.clearStencil(a), emscripten_glClientWaitSync:(a, b, c) => {
  c = Number(c);
  return L.clientWaitSync(dd[a], b, c);
}, emscripten_glClipControlEXT:(a, b) => {
  L.dc.clipControlEXT(a, b);
}, emscripten_glColorMask:Ae, emscripten_glCompileShader:Be, emscripten_glCompressedTexImage2D:(a, b, c, d, e, f, g, l) => {
  2 <= V.version ? L.T || !g ? L.compressedTexImage2D(a, b, c, d, e, f, g, l) : L.compressedTexImage2D(a, b, c, d, e, f, y, l, g) : L.compressedTexImage2D(a, b, c, d, e, f, y.subarray(l, l + g));
}, emscripten_glCompressedTexImage3D:(a, b, c, d, e, f, g, l, n) => {
  L.T ? L.compressedTexImage3D(a, b, c, d, e, f, g, l, n) : L.compressedTexImage3D(a, b, c, d, e, f, g, y, n, l);
}, emscripten_glCompressedTexSubImage2D:(a, b, c, d, e, f, g, l, n) => {
  2 <= V.version ? L.T || !l ? L.compressedTexSubImage2D(a, b, c, d, e, f, g, l, n) : L.compressedTexSubImage2D(a, b, c, d, e, f, g, y, n, l) : L.compressedTexSubImage2D(a, b, c, d, e, f, g, y.subarray(n, n + l));
}, emscripten_glCompressedTexSubImage3D:(a, b, c, d, e, f, g, l, n, r, t) => {
  L.T ? L.compressedTexSubImage3D(a, b, c, d, e, f, g, l, n, r, t) : L.compressedTexSubImage3D(a, b, c, d, e, f, g, l, n, y, t, r);
}, emscripten_glCopyBufferSubData:(a, b, c, d, e) => L.copyBufferSubData(a, b, c, d, e), emscripten_glCopyTexImage2D:(a, b, c, d, e, f, g, l) => L.copyTexImage2D(a, b, c, d, e, f, g, l), emscripten_glCopyTexSubImage2D:(a, b, c, d, e, f, g, l) => L.copyTexSubImage2D(a, b, c, d, e, f, g, l), emscripten_glCopyTexSubImage3D:(a, b, c, d, e, f, g, l, n) => L.copyTexSubImage3D(a, b, c, d, e, f, g, l, n), emscripten_glCreateProgram:Ce, emscripten_glCreateShader:De, emscripten_glCullFace:Ee, emscripten_glDeleteBuffers:Fe, 
emscripten_glDeleteFramebuffers:Ge, emscripten_glDeleteProgram:He, emscripten_glDeleteQueries:(a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = R[d];
    e && (L.deleteQuery(e), R[d] = null);
  }
}, emscripten_glDeleteQueriesEXT:(a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = R[d];
    e && (L.J.deleteQueryEXT(e), R[d] = null);
  }
}, emscripten_glDeleteRenderbuffers:Ie, emscripten_glDeleteSamplers:(a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = bd[d];
    e && (L.deleteSampler(e), e.name = 0, bd[d] = null);
  }
}, emscripten_glDeleteShader:Je, emscripten_glDeleteSync:a => {
  if (a) {
    var b = dd[a];
    b ? (L.deleteSync(b), b.name = 0, dd[a] = null) : T ||= 1281;
  }
}, emscripten_glDeleteTextures:Ke, emscripten_glDeleteTransformFeedbacks:(a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = cd[d];
    e && (L.deleteTransformFeedback(e), e.name = 0, cd[d] = null);
  }
}, emscripten_glDeleteVertexArrays:Le, emscripten_glDeleteVertexArraysOES:Le, emscripten_glDepthFunc:a => L.depthFunc(a), emscripten_glDepthMask:a => {
  L.depthMask(!!a);
}, emscripten_glDepthRangef:(a, b) => L.depthRange(a, b), emscripten_glDetachShader:(a, b) => {
  L.detachShader(P[a], Q[b]);
}, emscripten_glDisable:Me, emscripten_glDisableVertexAttribArray:Ne, emscripten_glDrawArrays:Oe, emscripten_glDrawArraysInstanced:Pe, emscripten_glDrawArraysInstancedANGLE:Pe, emscripten_glDrawArraysInstancedARB:Pe, emscripten_glDrawArraysInstancedEXT:Pe, emscripten_glDrawArraysInstancedNV:Pe, emscripten_glDrawBuffers:Re, emscripten_glDrawBuffersEXT:Re, emscripten_glDrawBuffersWEBGL:Re, emscripten_glDrawElements:Se, emscripten_glDrawElementsInstanced:Te, emscripten_glDrawElementsInstancedANGLE:Te, 
emscripten_glDrawElementsInstancedARB:Te, emscripten_glDrawElementsInstancedEXT:Te, emscripten_glDrawElementsInstancedNV:Te, emscripten_glDrawRangeElements:(a, b, c, d, e, f) => {
  Se(a, d, e, f);
}, emscripten_glEnable:Ue, emscripten_glEnableVertexAttribArray:Ve, emscripten_glEndQuery:a => L.endQuery(a), emscripten_glEndQueryEXT:a => {
  L.J.endQueryEXT(a);
}, emscripten_glEndTransformFeedback:() => L.endTransformFeedback(), emscripten_glFenceSync:(a, b) => (a = L.fenceSync(a, b)) ? (b = kd(dd), a.name = b, dd[b] = a, b) : 0, emscripten_glFinish:We, emscripten_glFlush:() => L.flush(), emscripten_glFlushMappedBufferRange:(a, b, c) => {
  if (Ye(a)) {
    var d = Xc[Xe(a)];
    d ? d.sb & 16 ? 0 > b || 0 > c || b + c > d.length ? (T ||= 1281, m("invalid range in glFlushMappedBufferRange")) : L.bufferSubData(a, d.offset, y.subarray(d.X + b, d.X + b + c)) : (T ||= 1282, m("buffer was not mapped with GL_MAP_FLUSH_EXPLICIT_BIT in glFlushMappedBufferRange")) : (T ||= 1282, m("buffer was never mapped in glFlushMappedBufferRange"));
  } else {
    T ||= 1280, m("GL_INVALID_ENUM in glFlushMappedBufferRange");
  }
}, emscripten_glFramebufferRenderbuffer:Ze, emscripten_glFramebufferTexture2D:$e, emscripten_glFramebufferTextureLayer:(a, b, c, d, e) => {
  L.framebufferTextureLayer(a, b, $c[c], d, e);
}, emscripten_glFrontFace:af, emscripten_glGenBuffers:bf, emscripten_glGenFramebuffers:cf, emscripten_glGenQueries:(a, b) => {
  ld(a, b, "createQuery", R);
}, emscripten_glGenQueriesEXT:(a, b) => {
  for (var c = 0; c < a; c++) {
    var d = L.J.createQueryEXT();
    if (!d) {
      for (T ||= 1282; c < a;) {
        z[b + 4 * c++ >> 2] = 0;
      }
      break;
    }
    var e = kd(R);
    d.name = e;
    R[e] = d;
    z[b + 4 * c >> 2] = e;
  }
}, emscripten_glGenRenderbuffers:df, emscripten_glGenSamplers:(a, b) => {
  ld(a, b, "createSampler", bd);
}, emscripten_glGenTextures:ef, emscripten_glGenTransformFeedbacks:(a, b) => {
  ld(a, b, "createTransformFeedback", cd);
}, emscripten_glGenVertexArrays:ff, emscripten_glGenVertexArraysOES:ff, emscripten_glGenerateMipmap:gf, emscripten_glGetActiveAttrib:(a, b, c, d, e, f, g) => hf("getActiveAttrib", a, b, c, d, e, f, g), emscripten_glGetActiveUniform:(a, b, c, d, e, f, g) => hf("getActiveUniform", a, b, c, d, e, f, g), emscripten_glGetActiveUniformBlockName:(a, b, c, d, e) => {
  a = P[a];
  if (a = L.getActiveUniformBlockName(a, b)) {
    e && 0 < c ? (c = K(a, e, c), d && (z[d >> 2] = c)) : d && (z[d >> 2] = 0);
  }
}, emscripten_glGetActiveUniformBlockiv:(a, b, c, d) => {
  if (d) {
    if (a = P[a], 35393 == c) {
      z[d >> 2] = L.getActiveUniformBlockName(a, b).length + 1;
    } else {
      if (a = L.getActiveUniformBlockParameter(a, b, c), null !== a) {
        if (35395 == c) {
          for (c = 0; c < a.length; c++) {
            z[d + 4 * c >> 2] = a[c];
          }
        } else {
          z[d >> 2] = a;
        }
      }
    }
  } else {
    T ||= 1281;
  }
}, emscripten_glGetActiveUniformsiv:(a, b, c, d, e) => {
  if (e) {
    if (0 < b && 0 == c) {
      T ||= 1281;
    } else {
      a = P[a];
      for (var f = [], g = 0; g < b; g++) {
        f.push(z[c + 4 * g >> 2]);
      }
      if (a = L.getActiveUniforms(a, f, d)) {
        for (b = a.length, g = 0; g < b; g++) {
          z[e + 4 * g >> 2] = a[g];
        }
      }
    }
  } else {
    T ||= 1281;
  }
}, emscripten_glGetAttachedShaders:(a, b, c, d) => {
  a = L.getAttachedShaders(P[a]);
  var e = a.length;
  e > b && (e = b);
  z[c >> 2] = e;
  for (b = 0; b < e; ++b) {
    z[d + 4 * b >> 2] = Q.indexOf(a[b]);
  }
}, emscripten_glGetAttribLocation:(a, b) => L.getAttribLocation(P[a], D(b)), emscripten_glGetBooleanv:(a, b) => lf(a, b, 4), emscripten_glGetBufferParameteri64v:(a, b, c) => {
  c ? jf(c, L.getBufferParameter(a, b)) : T ||= 1281;
}, emscripten_glGetBufferParameteriv:(a, b, c) => {
  c ? z[c >> 2] = L.getBufferParameter(a, b) : T ||= 1281;
}, emscripten_glGetBufferPointerv:(a, b, c) => {
  if (35005 == b) {
    b = 0;
    if (a = Xc[Xe(a)]) {
      b = a.X;
    }
    z[c >> 2] = b;
  } else {
    T ||= 1280, m("GL_INVALID_ENUM in glGetBufferPointerv");
  }
}, emscripten_glGetError:mf, emscripten_glGetFloatv:(a, b) => lf(a, b, 2), emscripten_glGetFragDataLocation:(a, b) => L.getFragDataLocation(P[a], D(b)), emscripten_glGetFramebufferAttachmentParameteriv:(a, b, c, d) => {
  a = L.getFramebufferAttachmentParameter(a, b, c);
  if (a instanceof WebGLRenderbuffer || a instanceof WebGLTexture) {
    a = a.name | 0;
  }
  z[d >> 2] = a;
}, emscripten_glGetInteger64i_v:(a, b, c) => nf(a, b, c, 1), emscripten_glGetInteger64v:(a, b) => {
  lf(a, b, 1);
}, emscripten_glGetIntegeri_v:(a, b, c) => nf(a, b, c, 0), emscripten_glGetIntegerv:of, emscripten_glGetInternalformativ:(a, b, c, d, e) => {
  if (0 > d) {
    T ||= 1281;
  } else {
    if (e) {
      if (a = L.getInternalformatParameter(a, b, c), null !== a) {
        for (b = 0; b < a.length && b < d; ++b) {
          z[e + 4 * b >> 2] = a[b];
        }
      }
    } else {
      T ||= 1281;
    }
  }
}, emscripten_glGetProgramBinary:() => {
  T ||= 1282;
}, emscripten_glGetProgramInfoLog:pf, emscripten_glGetProgramiv:qf, emscripten_glGetQueryObjecti64vEXT:rf, emscripten_glGetQueryObjectivEXT:sf, emscripten_glGetQueryObjectui64vEXT:rf, emscripten_glGetQueryObjectuiv:(a, b, c) => {
  if (c) {
    a = L.getQueryParameter(R[a], b);
    var d;
    "boolean" == typeof a ? d = a ? 1 : 0 : d = a;
    z[c >> 2] = d;
  } else {
    T ||= 1281;
  }
}, emscripten_glGetQueryObjectuivEXT:sf, emscripten_glGetQueryiv:(a, b, c) => {
  c ? z[c >> 2] = L.getQuery(a, b) : T ||= 1281;
}, emscripten_glGetQueryivEXT:(a, b, c) => {
  c ? z[c >> 2] = L.J.getQueryEXT(a, b) : T ||= 1281;
}, emscripten_glGetRenderbufferParameteriv:(a, b, c) => {
  c ? z[c >> 2] = L.getRenderbufferParameter(a, b) : T ||= 1281;
}, emscripten_glGetSamplerParameterfv:(a, b, c) => {
  c ? A[c >> 2] = L.getSamplerParameter(bd[a], b) : T ||= 1281;
}, emscripten_glGetSamplerParameteriv:(a, b, c) => {
  c ? z[c >> 2] = L.getSamplerParameter(bd[a], b) : T ||= 1281;
}, emscripten_glGetShaderInfoLog:tf, emscripten_glGetShaderPrecisionFormat:(a, b, c, d) => {
  a = L.getShaderPrecisionFormat(a, b);
  z[c >> 2] = a.rangeMin;
  z[c + 4 >> 2] = a.rangeMax;
  z[d >> 2] = a.precision;
}, emscripten_glGetShaderSource:(a, b, c, d) => {
  if (a = L.getShaderSource(Q[a])) {
    b = 0 < b && d ? K(a, d, b) : 0, c && (z[c >> 2] = b);
  }
}, emscripten_glGetShaderiv:uf, emscripten_glGetString:a => {
  var b = fd[a];
  if (!b) {
    switch(a) {
      case 7939:
        b = rd(kf().join(" "));
        break;
      case 7936:
      case 7937:
      case 37445:
      case 37446:
        (b = L.getParameter(a)) || (T ||= 1280);
        b = b ? rd(b) : 0;
        break;
      case 7938:
        b = L.getParameter(7938);
        var c = `OpenGL ES 2.0 (${b})`;
        2 <= V.version && (c = `OpenGL ES 3.0 (${b})`);
        b = rd(c);
        break;
      case 35724:
        b = L.getParameter(35724);
        c = b.match(/^WebGL GLSL ES ([0-9]\.[0-9][0-9]?)(?:$| .*)/);
        null !== c && (3 == c[1].length && (c[1] += "0"), b = `OpenGL ES GLSL ES ${c[1]} (${b})`);
        b = rd(b);
        break;
      default:
        T ||= 1280;
    }
    fd[a] = b;
  }
  return b;
}, emscripten_glGetStringi:(a, b) => {
  if (2 > V.version) {
    return T ||= 1282, 0;
  }
  var c = gd[a];
  if (c) {
    return 0 > b || b >= c.length ? (T ||= 1281, 0) : c[b];
  }
  switch(a) {
    case 7939:
      return c = kf().map(rd), c = gd[a] = c, 0 > b || b >= c.length ? (T ||= 1281, 0) : c[b];
    default:
      return T ||= 1280, 0;
  }
}, emscripten_glGetSynciv:(a, b, c, d, e) => {
  0 > c ? T ||= 1281 : e ? (a = L.getSyncParameter(dd[a], b), null !== a && (z[e >> 2] = a, d && (z[d >> 2] = 1))) : T ||= 1281;
}, emscripten_glGetTexParameterfv:(a, b, c) => {
  c ? A[c >> 2] = L.getTexParameter(a, b) : T ||= 1281;
}, emscripten_glGetTexParameteriv:(a, b, c) => {
  c ? z[c >> 2] = L.getTexParameter(a, b) : T ||= 1281;
}, emscripten_glGetTransformFeedbackVarying:(a, b, c, d, e, f, g) => {
  a = P[a];
  if (a = L.getTransformFeedbackVarying(a, b)) {
    g && 0 < c ? (c = K(a.name, g, c), d && (z[d >> 2] = c)) : d && (z[d >> 2] = 0), e && (z[e >> 2] = a.size), f && (z[f >> 2] = a.type);
  }
}, emscripten_glGetUniformBlockIndex:(a, b) => L.getUniformBlockIndex(P[a], D(b)), emscripten_glGetUniformIndices:(a, b, c, d) => {
  if (d) {
    if (0 < b && (0 == c || 0 == d)) {
      T ||= 1281;
    } else {
      a = P[a];
      for (var e = [], f = 0; f < b; f++) {
        e.push(D(q[c + 4 * f >> 2]));
      }
      if (a = L.getUniformIndices(a, e)) {
        for (b = a.length, f = 0; f < b; f++) {
          z[d + 4 * f >> 2] = a[f];
        }
      }
    }
  } else {
    T ||= 1281;
  }
}, emscripten_glGetUniformLocation:xf, emscripten_glGetUniformfv:(a, b, c) => {
  yf(a, b, c, 2);
}, emscripten_glGetUniformiv:(a, b, c) => {
  yf(a, b, c, 0);
}, emscripten_glGetUniformuiv:(a, b, c) => yf(a, b, c, 0), emscripten_glGetVertexAttribIiv:Af, emscripten_glGetVertexAttribIuiv:Af, emscripten_glGetVertexAttribPointerv:(a, b, c) => {
  c ? (V.W[a].enabled && m("glGetVertexAttribPointer on client-side array: not supported, bad data returned"), z[c >> 2] = L.getVertexAttribOffset(a, b)) : T ||= 1281;
}, emscripten_glGetVertexAttribfv:(a, b, c) => {
  zf(a, b, c, 2);
}, emscripten_glGetVertexAttribiv:(a, b, c) => {
  zf(a, b, c, 5);
}, emscripten_glHint:(a, b) => L.hint(a, b), emscripten_glInvalidateFramebuffer:(a, b, c) => {
  for (var d = Qe[b], e = 0; e < b; e++) {
    d[e] = z[c + 4 * e >> 2];
  }
  L.invalidateFramebuffer(a, d);
}, emscripten_glInvalidateSubFramebuffer:(a, b, c, d, e, f, g) => {
  for (var l = Qe[b], n = 0; n < b; n++) {
    l[n] = z[c + 4 * n >> 2];
  }
  L.invalidateSubFramebuffer(a, l, d, e, f, g);
}, emscripten_glIsBuffer:a => (a = Wc[a]) ? L.isBuffer(a) : 0, emscripten_glIsEnabled:a => L.isEnabled(a), emscripten_glIsFramebuffer:a => (a = Yc[a]) ? L.isFramebuffer(a) : 0, emscripten_glIsProgram:a => (a = P[a]) ? L.isProgram(a) : 0, emscripten_glIsQuery:a => (a = R[a]) ? L.isQuery(a) : 0, emscripten_glIsQueryEXT:a => (a = R[a]) ? L.J.isQueryEXT(a) : 0, emscripten_glIsRenderbuffer:a => (a = Zc[a]) ? L.isRenderbuffer(a) : 0, emscripten_glIsSampler:a => (a = bd[a]) ? L.isSampler(a) : 0, emscripten_glIsShader:a => 
(a = Q[a]) ? L.isShader(a) : 0, emscripten_glIsSync:a => L.isSync(dd[a]), emscripten_glIsTexture:a => (a = $c[a]) ? L.isTexture(a) : 0, emscripten_glIsTransformFeedback:a => L.isTransformFeedback(cd[a]), emscripten_glIsVertexArray:Bf, emscripten_glIsVertexArrayOES:Bf, emscripten_glLineWidth:a => L.lineWidth(a), emscripten_glLinkProgram:Cf, emscripten_glMapBufferRange:(a, b, c, d) => {
  if (0 != (d & 33)) {
    return m("glMapBufferRange access does not support MAP_READ or MAP_UNSYNCHRONIZED"), 0;
  }
  if (0 == (d & 2)) {
    return m("glMapBufferRange access must include MAP_WRITE"), 0;
  }
  if (0 == (d & 12)) {
    return m("glMapBufferRange access must include INVALIDATE_BUFFER or INVALIDATE_RANGE"), 0;
  }
  if (!Ye(a)) {
    return T ||= 1280, m("GL_INVALID_ENUM in glMapBufferRange"), 0;
  }
  var e = qd(c);
  a = Xe(a);
  if (!e) {
    return 0;
  }
  let f;
  a = Xc[f = a] ?? (Xc[f] = {});
  a.offset = b;
  a.length = c;
  a.X = e;
  a.sb = d;
  return e;
}, emscripten_glPauseTransformFeedback:() => L.pauseTransformFeedback(), emscripten_glPixelStorei:Df, emscripten_glPolygonModeWEBGL:(a, b) => {
  L.Oc.polygonModeWEBGL(a, b);
}, emscripten_glPolygonOffset:(a, b) => L.polygonOffset(a, b), emscripten_glPolygonOffsetClampEXT:(a, b, c) => {
  L.ec.polygonOffsetClampEXT(a, b, c);
}, emscripten_glProgramBinary:() => {
  T ||= 1280;
}, emscripten_glProgramParameteri:() => {
  T ||= 1280;
}, emscripten_glQueryCounterEXT:(a, b) => {
  L.J.queryCounterEXT(R[a], b);
}, emscripten_glReadBuffer:a => L.readBuffer(a), emscripten_glReadPixels:Gf, emscripten_glReleaseShaderCompiler:() => {
}, emscripten_glRenderbufferStorage:Hf, emscripten_glRenderbufferStorageMultisample:(a, b, c, d, e) => L.renderbufferStorageMultisample(a, b, c, d, e), emscripten_glResumeTransformFeedback:() => L.resumeTransformFeedback(), emscripten_glSampleCoverage:(a, b) => {
  L.sampleCoverage(a, !!b);
}, emscripten_glSamplerParameterf:(a, b, c) => {
  L.samplerParameterf(bd[a], b, c);
}, emscripten_glSamplerParameterfv:(a, b, c) => {
  L.samplerParameterf(bd[a], b, A[c >> 2]);
}, emscripten_glSamplerParameteri:(a, b, c) => {
  L.samplerParameteri(bd[a], b, c);
}, emscripten_glSamplerParameteriv:(a, b, c) => {
  L.samplerParameteri(bd[a], b, z[c >> 2]);
}, emscripten_glScissor:(a, b, c, d) => L.scissor(a, b, c, d), emscripten_glShaderBinary:() => {
  T ||= 1280;
}, emscripten_glShaderSource:If, emscripten_glStencilFunc:Jf, emscripten_glStencilFuncSeparate:(a, b, c, d) => L.stencilFuncSeparate(a, b, c, d), emscripten_glStencilMask:Kf, emscripten_glStencilMaskSeparate:(a, b) => L.stencilMaskSeparate(a, b), emscripten_glStencilOp:Lf, emscripten_glStencilOpSeparate:Mf, emscripten_glTexImage2D:Nf, emscripten_glTexImage3D:(a, b, c, d, e, f, g, l, n, r) => {
  if (L.T) {
    L.texImage3D(a, b, c, d, e, f, g, l, n, r);
  } else if (r) {
    var t = Ef(n);
    L.texImage3D(a, b, c, d, e, f, g, l, n, t, r >>> 31 - Math.clz32(t.BYTES_PER_ELEMENT));
  } else {
    L.texImage3D(a, b, c, d, e, f, g, l, n, null);
  }
}, emscripten_glTexParameterf:(a, b, c) => L.texParameterf(a, b, c), emscripten_glTexParameterfv:(a, b, c) => {
  L.texParameterf(a, b, A[c >> 2]);
}, emscripten_glTexParameteri:Of, emscripten_glTexParameteriv:(a, b, c) => {
  L.texParameteri(a, b, z[c >> 2]);
}, emscripten_glTexStorage2D:(a, b, c, d, e) => L.texStorage2D(a, b, c, d, e), emscripten_glTexStorage3D:(a, b, c, d, e, f) => L.texStorage3D(a, b, c, d, e, f), emscripten_glTexSubImage2D:Pf, emscripten_glTexSubImage3D:(a, b, c, d, e, f, g, l, n, r, t) => {
  if (L.T) {
    L.texSubImage3D(a, b, c, d, e, f, g, l, n, r, t);
  } else if (t) {
    var v = Ef(r);
    L.texSubImage3D(a, b, c, d, e, f, g, l, n, r, v, t >>> 31 - Math.clz32(v.BYTES_PER_ELEMENT));
  } else {
    L.texSubImage3D(a, b, c, d, e, f, g, l, n, r, null);
  }
}, emscripten_glTransformFeedbackVaryings:(a, b, c, d) => {
  a = P[a];
  for (var e = [], f = 0; f < b; f++) {
    e.push(D(q[c + 4 * f >> 2]));
  }
  L.transformFeedbackVaryings(a, e, d);
}, emscripten_glUniform1f:(a, b) => {
  L.uniform1f(Z(a), b);
}, emscripten_glUniform1fv:(a, b, c) => {
  if (2 <= V.version) {
    b && L.uniform1fv(Z(a), A, c >> 2, b);
  } else {
    if (288 >= b) {
      for (var d = Qf[b], e = 0; e < b; ++e) {
        d[e] = A[c + 4 * e >> 2];
      }
    } else {
      d = A.subarray(c >> 2, c + 4 * b >> 2);
    }
    L.uniform1fv(Z(a), d);
  }
}, emscripten_glUniform1i:Rf, emscripten_glUniform1iv:(a, b, c) => {
  if (2 <= V.version) {
    b && L.uniform1iv(Z(a), z, c >> 2, b);
  } else {
    if (288 >= b) {
      for (var d = Sf[b], e = 0; e < b; ++e) {
        d[e] = z[c + 4 * e >> 2];
      }
    } else {
      d = z.subarray(c >> 2, c + 4 * b >> 2);
    }
    L.uniform1iv(Z(a), d);
  }
}, emscripten_glUniform1ui:(a, b) => {
  L.uniform1ui(Z(a), b);
}, emscripten_glUniform1uiv:(a, b, c) => {
  b && L.uniform1uiv(Z(a), q, c >> 2, b);
}, emscripten_glUniform2f:(a, b, c) => {
  L.uniform2f(Z(a), b, c);
}, emscripten_glUniform2fv:Tf, emscripten_glUniform2i:(a, b, c) => {
  L.uniform2i(Z(a), b, c);
}, emscripten_glUniform2iv:(a, b, c) => {
  if (2 <= V.version) {
    b && L.uniform2iv(Z(a), z, c >> 2, 2 * b);
  } else {
    if (144 >= b) {
      b *= 2;
      for (var d = Sf[b], e = 0; e < b; e += 2) {
        d[e] = z[c + 4 * e >> 2], d[e + 1] = z[c + (4 * e + 4) >> 2];
      }
    } else {
      d = z.subarray(c >> 2, c + 8 * b >> 2);
    }
    L.uniform2iv(Z(a), d);
  }
}, emscripten_glUniform2ui:(a, b, c) => {
  L.uniform2ui(Z(a), b, c);
}, emscripten_glUniform2uiv:(a, b, c) => {
  b && L.uniform2uiv(Z(a), q, c >> 2, 2 * b);
}, emscripten_glUniform3f:(a, b, c, d) => {
  L.uniform3f(Z(a), b, c, d);
}, emscripten_glUniform3fv:(a, b, c) => {
  if (2 <= V.version) {
    b && L.uniform3fv(Z(a), A, c >> 2, 3 * b);
  } else {
    if (96 >= b) {
      b *= 3;
      for (var d = Qf[b], e = 0; e < b; e += 3) {
        d[e] = A[c + 4 * e >> 2], d[e + 1] = A[c + (4 * e + 4) >> 2], d[e + 2] = A[c + (4 * e + 8) >> 2];
      }
    } else {
      d = A.subarray(c >> 2, c + 12 * b >> 2);
    }
    L.uniform3fv(Z(a), d);
  }
}, emscripten_glUniform3i:(a, b, c, d) => {
  L.uniform3i(Z(a), b, c, d);
}, emscripten_glUniform3iv:(a, b, c) => {
  if (2 <= V.version) {
    b && L.uniform3iv(Z(a), z, c >> 2, 3 * b);
  } else {
    if (96 >= b) {
      b *= 3;
      for (var d = Sf[b], e = 0; e < b; e += 3) {
        d[e] = z[c + 4 * e >> 2], d[e + 1] = z[c + (4 * e + 4) >> 2], d[e + 2] = z[c + (4 * e + 8) >> 2];
      }
    } else {
      d = z.subarray(c >> 2, c + 12 * b >> 2);
    }
    L.uniform3iv(Z(a), d);
  }
}, emscripten_glUniform3ui:(a, b, c, d) => {
  L.uniform3ui(Z(a), b, c, d);
}, emscripten_glUniform3uiv:(a, b, c) => {
  b && L.uniform3uiv(Z(a), q, c >> 2, 3 * b);
}, emscripten_glUniform4f:(a, b, c, d, e) => {
  L.uniform4f(Z(a), b, c, d, e);
}, emscripten_glUniform4fv:Uf, emscripten_glUniform4i:(a, b, c, d, e) => {
  L.uniform4i(Z(a), b, c, d, e);
}, emscripten_glUniform4iv:(a, b, c) => {
  if (2 <= V.version) {
    b && L.uniform4iv(Z(a), z, c >> 2, 4 * b);
  } else {
    if (72 >= b) {
      b *= 4;
      for (var d = Sf[b], e = 0; e < b; e += 4) {
        d[e] = z[c + 4 * e >> 2], d[e + 1] = z[c + (4 * e + 4) >> 2], d[e + 2] = z[c + (4 * e + 8) >> 2], d[e + 3] = z[c + (4 * e + 12) >> 2];
      }
    } else {
      d = z.subarray(c >> 2, c + 16 * b >> 2);
    }
    L.uniform4iv(Z(a), d);
  }
}, emscripten_glUniform4ui:(a, b, c, d, e) => {
  L.uniform4ui(Z(a), b, c, d, e);
}, emscripten_glUniform4uiv:(a, b, c) => {
  b && L.uniform4uiv(Z(a), q, c >> 2, 4 * b);
}, emscripten_glUniformBlockBinding:(a, b, c) => {
  a = P[a];
  L.uniformBlockBinding(a, b, c);
}, emscripten_glUniformMatrix2fv:(a, b, c, d) => {
  if (2 <= V.version) {
    b && L.uniformMatrix2fv(Z(a), !!c, A, d >> 2, 4 * b);
  } else {
    if (72 >= b) {
      b *= 4;
      for (var e = Qf[b], f = 0; f < b; f += 4) {
        e[f] = A[d + 4 * f >> 2], e[f + 1] = A[d + (4 * f + 4) >> 2], e[f + 2] = A[d + (4 * f + 8) >> 2], e[f + 3] = A[d + (4 * f + 12) >> 2];
      }
    } else {
      e = A.subarray(d >> 2, d + 16 * b >> 2);
    }
    L.uniformMatrix2fv(Z(a), !!c, e);
  }
}, emscripten_glUniformMatrix2x3fv:(a, b, c, d) => {
  b && L.uniformMatrix2x3fv(Z(a), !!c, A, d >> 2, 6 * b);
}, emscripten_glUniformMatrix2x4fv:(a, b, c, d) => {
  b && L.uniformMatrix2x4fv(Z(a), !!c, A, d >> 2, 8 * b);
}, emscripten_glUniformMatrix3fv:(a, b, c, d) => {
  if (2 <= V.version) {
    b && L.uniformMatrix3fv(Z(a), !!c, A, d >> 2, 9 * b);
  } else {
    if (32 >= b) {
      b *= 9;
      for (var e = Qf[b], f = 0; f < b; f += 9) {
        e[f] = A[d + 4 * f >> 2], e[f + 1] = A[d + (4 * f + 4) >> 2], e[f + 2] = A[d + (4 * f + 8) >> 2], e[f + 3] = A[d + (4 * f + 12) >> 2], e[f + 4] = A[d + (4 * f + 16) >> 2], e[f + 5] = A[d + (4 * f + 20) >> 2], e[f + 6] = A[d + (4 * f + 24) >> 2], e[f + 7] = A[d + (4 * f + 28) >> 2], e[f + 8] = A[d + (4 * f + 32) >> 2];
      }
    } else {
      e = A.subarray(d >> 2, d + 36 * b >> 2);
    }
    L.uniformMatrix3fv(Z(a), !!c, e);
  }
}, emscripten_glUniformMatrix3x2fv:(a, b, c, d) => {
  b && L.uniformMatrix3x2fv(Z(a), !!c, A, d >> 2, 6 * b);
}, emscripten_glUniformMatrix3x4fv:(a, b, c, d) => {
  b && L.uniformMatrix3x4fv(Z(a), !!c, A, d >> 2, 12 * b);
}, emscripten_glUniformMatrix4fv:(a, b, c, d) => {
  if (2 <= V.version) {
    b && L.uniformMatrix4fv(Z(a), !!c, A, d >> 2, 16 * b);
  } else {
    if (18 >= b) {
      var e = Qf[16 * b], f = A;
      d >>= 2;
      b *= 16;
      for (var g = 0; g < b; g += 16) {
        var l = d + g;
        e[g] = f[l];
        e[g + 1] = f[l + 1];
        e[g + 2] = f[l + 2];
        e[g + 3] = f[l + 3];
        e[g + 4] = f[l + 4];
        e[g + 5] = f[l + 5];
        e[g + 6] = f[l + 6];
        e[g + 7] = f[l + 7];
        e[g + 8] = f[l + 8];
        e[g + 9] = f[l + 9];
        e[g + 10] = f[l + 10];
        e[g + 11] = f[l + 11];
        e[g + 12] = f[l + 12];
        e[g + 13] = f[l + 13];
        e[g + 14] = f[l + 14];
        e[g + 15] = f[l + 15];
      }
    } else {
      e = A.subarray(d >> 2, d + 64 * b >> 2);
    }
    L.uniformMatrix4fv(Z(a), !!c, e);
  }
}, emscripten_glUniformMatrix4x2fv:(a, b, c, d) => {
  b && L.uniformMatrix4x2fv(Z(a), !!c, A, d >> 2, 8 * b);
}, emscripten_glUniformMatrix4x3fv:(a, b, c, d) => {
  b && L.uniformMatrix4x3fv(Z(a), !!c, A, d >> 2, 12 * b);
}, emscripten_glUnmapBuffer:a => {
  if (!Ye(a)) {
    return T ||= 1280, m("GL_INVALID_ENUM in glUnmapBuffer"), 0;
  }
  var b = Xc[Xe(a)];
  if (!b || !b.X) {
    return T ||= 1282, m("buffer was never mapped in glUnmapBuffer"), 0;
  }
  b.sb & 16 || (2 <= V.version ? L.bufferSubData(a, b.offset, y, b.X, b.length) : L.bufferSubData(a, b.offset, y.subarray(b.X, b.X + b.length)));
  Qg(b.X);
  b.X = 0;
  return 1;
}, emscripten_glUseProgram:Vf, emscripten_glValidateProgram:a => {
  L.validateProgram(P[a]);
}, emscripten_glVertexAttrib1f:(a, b) => L.vertexAttrib1f(a, b), emscripten_glVertexAttrib1fv:(a, b) => {
  L.vertexAttrib1f(a, A[b >> 2]);
}, emscripten_glVertexAttrib2f:(a, b, c) => L.vertexAttrib2f(a, b, c), emscripten_glVertexAttrib2fv:(a, b) => {
  L.vertexAttrib2f(a, A[b >> 2], A[b + 4 >> 2]);
}, emscripten_glVertexAttrib3f:(a, b, c, d) => L.vertexAttrib3f(a, b, c, d), emscripten_glVertexAttrib3fv:(a, b) => {
  L.vertexAttrib3f(a, A[b >> 2], A[b + 4 >> 2], A[b + 8 >> 2]);
}, emscripten_glVertexAttrib4f:(a, b, c, d, e) => L.vertexAttrib4f(a, b, c, d, e), emscripten_glVertexAttrib4fv:(a, b) => {
  L.vertexAttrib4f(a, A[b >> 2], A[b + 4 >> 2], A[b + 8 >> 2], A[b + 12 >> 2]);
}, emscripten_glVertexAttribDivisor:Wf, emscripten_glVertexAttribDivisorANGLE:Wf, emscripten_glVertexAttribDivisorARB:Wf, emscripten_glVertexAttribDivisorEXT:Wf, emscripten_glVertexAttribDivisorNV:Wf, emscripten_glVertexAttribI4i:(a, b, c, d, e) => L.vertexAttribI4i(a, b, c, d, e), emscripten_glVertexAttribI4iv:(a, b) => {
  L.vertexAttribI4i(a, z[b >> 2], z[b + 4 >> 2], z[b + 8 >> 2], z[b + 12 >> 2]);
}, emscripten_glVertexAttribI4ui:(a, b, c, d, e) => L.vertexAttribI4ui(a, b, c, d, e), emscripten_glVertexAttribI4uiv:(a, b) => {
  L.vertexAttribI4ui(a, q[b >> 2], q[b + 4 >> 2], q[b + 8 >> 2], q[b + 12 >> 2]);
}, emscripten_glVertexAttribIPointer:(a, b, c, d, e) => {
  var f = V.W[a];
  L.ka ? (f.ja = !1, L.vertexAttribIPointer(a, b, c, d, e)) : (f.size = b, f.type = c, f.gb = !1, f.stride = d, f.oa = e, f.ja = !0, f.pb = function(g, l, n, r, t, v) {
    this.vertexAttribIPointer(g, l, n, t, v);
  });
}, emscripten_glVertexAttribPointer:Xf, emscripten_glViewport:Yf, emscripten_glWaitSync:(a, b, c) => {
  c = Number(c);
  L.waitSync(dd[a], b, c);
}, emscripten_has_asyncify:() => 1, emscripten_request_fullscreen_strategy:(a, b, c) => Zf(a, {jb:z[c >> 2], Na:z[c + 4 >> 2], hc:z[c + 8 >> 2], $b:b, Ba:z[c + 12 >> 2], ub:z[c + 16 >> 2]}), emscripten_request_pointerlock:(a, b) => {
  a = Y(a);
  return a ? a.requestPointerLock ? Vd() ? le(a) : b ? (Td(le, 2, [a]), 1) : -2 : -1 : -4;
}, emscripten_resize_heap:a => {
  var b = y.length;
  a >>>= 0;
  k(a > b);
  if (2147483648 < a) {
    return m(`Cannot enlarge memory, requested ${a} bytes, but the limit is ${2147483648} bytes!`), !1;
  }
  for (var c = 1; 4 >= c; c *= 2) {
    var d = b * (1 + 0.2 / c);
    d = Math.min(d, a + 100663296);
    var e = Math, f = e.min;
    d = Math.max(a, d);
    k(65536, "alignment argument is required");
    e = f.call(e, 2147483648, 65536 * Math.ceil(d / 65536));
    a: {
      f = e;
      d = Ja.buffer.byteLength;
      try {
        Ja.grow((f - d + 65535) / 65536 | 0);
        Ia();
        var g = 1;
        break a;
      } catch (l) {
        m(`growMemory: Attempted to grow heap from ${d} bytes to ${f} bytes, but got error: ${l}`);
      }
      g = void 0;
    }
    if (g) {
      return !0;
    }
  }
  m(`Failed to grow the heap from ${b} bytes to ${e} bytes, not enough memory!`);
  return !1;
}, emscripten_sample_gamepad_data:$f, emscripten_set_beforeunload_callback_on_thread:(a, b, c) => "undefined" == typeof onbeforeunload ? -1 : 1 !== c ? -5 : ag(a, b), emscripten_set_blur_callback_on_thread:(a, b, c, d) => bg(a, b, c, d, 12, "blur"), emscripten_set_canvas_element_size:de, emscripten_set_element_css_size:(a, b, c) => {
  a = Y(a);
  if (!a) {
    return -4;
  }
  a.style.width = b + "px";
  a.style.height = c + "px";
  return 0;
}, emscripten_set_focus_callback_on_thread:(a, b, c, d) => bg(a, b, c, d, 13, "focus"), emscripten_set_fullscreenchange_callback_on_thread:(a, b, c, d) => {
  if (!X.fullscreenEnabled()) {
    return -1;
  }
  a = Y(a);
  if (!a) {
    return -4;
  }
  cg(a, b, c, d, "webkitfullscreenchange");
  return cg(a, b, c, d, "fullscreenchange");
}, emscripten_set_gamepadconnected_callback_on_thread:(a, b, c) => $f() ? -1 : dg(a, b, c, 26, "gamepadconnected"), emscripten_set_gamepaddisconnected_callback_on_thread:(a, b, c) => $f() ? -1 : dg(a, b, c, 27, "gamepaddisconnected"), emscripten_set_keydown_callback_on_thread:(a, b, c, d) => eg(a, b, c, d, 2, "keydown"), emscripten_set_keypress_callback_on_thread:(a, b, c, d) => eg(a, b, c, d, 1, "keypress"), emscripten_set_keyup_callback_on_thread:(a, b, c, d) => eg(a, b, c, d, 3, "keyup"), emscripten_set_main_loop:(a, 
b, c) => {
  Id(() => dynCall_v(a), b, c);
}, emscripten_set_mousedown_callback_on_thread:(a, b, c, d) => gg(a, b, c, d, 5, "mousedown"), emscripten_set_mouseenter_callback_on_thread:(a, b, c, d) => gg(a, b, c, d, 33, "mouseenter"), emscripten_set_mouseleave_callback_on_thread:(a, b, c, d) => gg(a, b, c, d, 34, "mouseleave"), emscripten_set_mousemove_callback_on_thread:(a, b, c, d) => gg(a, b, c, d, 8, "mousemove"), emscripten_set_mouseup_callback_on_thread:(a, b, c, d) => gg(a, b, c, d, 6, "mouseup"), emscripten_set_pointerlockchange_callback_on_thread:(a, 
b, c, d) => document.body?.requestPointerLock ? (a = Y(a)) ? hg(a, b, c, d) : -4 : -1, emscripten_set_resize_callback_on_thread:(a, b, c, d) => ig(a, b, c, d), emscripten_set_touchcancel_callback_on_thread:(a, b, c, d) => jg(a, b, c, d, 25, "touchcancel"), emscripten_set_touchend_callback_on_thread:(a, b, c, d) => jg(a, b, c, d, 23, "touchend"), emscripten_set_touchmove_callback_on_thread:(a, b, c, d) => jg(a, b, c, d, 24, "touchmove"), emscripten_set_touchstart_callback_on_thread:(a, b, c, d) => 
jg(a, b, c, d, 22, "touchstart"), emscripten_set_visibilitychange_callback_on_thread:(a, b, c) => Zd[1] ? kg(a, b, c) : -4, emscripten_set_wheel_callback_on_thread:(a, b, c, d) => (a = Y(a)) ? "undefined" != typeof a.onwheel ? lg(a, b, c, d) : -1 : -4, emscripten_set_window_title:a => document.title = D(a), emscripten_sleep:mg, environ_get:(a, b) => {
  var c = 0, d = 0, e;
  for (e of qg()) {
    var f = b + c;
    q[a + d >> 2] = f;
    c += K(e, f, Infinity) + 1;
    d += 4;
  }
  return 0;
}, environ_sizes_get:(a, b) => {
  var c = qg();
  q[a >> 2] = c.length;
  a = 0;
  for (var d of c) {
    a += ub(d) + 1;
  }
  q[b >> 2] = a;
  return 0;
}, fd_close:function(a) {
  try {
    var b = Yb(a);
    E.close(b);
    return 0;
  } catch (c) {
    if ("undefined" == typeof E || "ErrnoError" !== c.name) {
      throw c;
    }
    return c.s;
  }
}, fd_read:function(a, b, c, d) {
  try {
    a: {
      var e = Yb(a);
      a = b;
      for (var f, g = b = 0; g < c; g++) {
        var l = q[a >> 2], n = q[a + 4 >> 2];
        a += 8;
        var r = E.read(e, x, l, n, f);
        if (0 > r) {
          var t = -1;
          break a;
        }
        b += r;
        if (r < n) {
          break;
        }
        "undefined" != typeof f && (f += r);
      }
      t = b;
    }
    q[d >> 2] = t;
    return 0;
  } catch (v) {
    if ("undefined" == typeof E || "ErrnoError" !== v.name) {
      throw v;
    }
    return v.s;
  }
}, fd_seek:function(a, b, c, d) {
  b = -9007199254740992 > b || 9007199254740992 < b ? NaN : Number(b);
  try {
    if (isNaN(b)) {
      return 61;
    }
    var e = Yb(a);
    E.D(e, b, c);
    Na[d >> 3] = BigInt(e.position);
    e.Ua && 0 === b && 0 === c && (e.Ua = null);
    return 0;
  } catch (f) {
    if ("undefined" == typeof E || "ErrnoError" !== f.name) {
      throw f;
    }
    return f.s;
  }
}, fd_write:function(a, b, c, d) {
  try {
    a: {
      var e = Yb(a);
      a = b;
      for (var f, g = b = 0; g < c; g++) {
        var l = q[a >> 2], n = q[a + 4 >> 2];
        a += 8;
        var r = E.write(e, x, l, n, f);
        if (0 > r) {
          var t = -1;
          break a;
        }
        b += r;
        if (r < n) {
          break;
        }
        "undefined" != typeof f && (f += r);
      }
      t = b;
    }
    q[d >> 2] = t;
    return 0;
  } catch (v) {
    if ("undefined" == typeof E || "ErrnoError" !== v.name) {
      throw v;
    }
    return v.s;
  }
}, glActiveTexture:ne, glAttachShader:oe, glBindAttribLocation:pe, glBindBuffer:qe, glBindFramebuffer:re, glBindRenderbuffer:se, glBindTexture:te, glBlendFuncSeparate:ve, glBufferData:we, glCheckFramebufferStatus:xe, glClear:ye, glClearColor:ze, glColorMask:Ae, glCompileShader:Be, glCreateProgram:Ce, glCreateShader:De, glCullFace:Ee, glDeleteBuffers:Fe, glDeleteFramebuffers:Ge, glDeleteProgram:He, glDeleteRenderbuffers:Ie, glDeleteShader:Je, glDeleteTextures:Ke, glDisable:Me, glDisableVertexAttribArray:Ne, 
glDrawArrays:Oe, glEnable:Ue, glEnableVertexAttribArray:Ve, glFinish:We, glFramebufferRenderbuffer:Ze, glFramebufferTexture2D:$e, glFrontFace:af, glGenBuffers:bf, glGenFramebuffers:cf, glGenRenderbuffers:df, glGenTextures:ef, glGenerateMipmap:gf, glGetError:mf, glGetIntegerv:of, glGetProgramInfoLog:pf, glGetProgramiv:qf, glGetShaderInfoLog:tf, glGetShaderiv:uf, glGetUniformLocation:xf, glLinkProgram:Cf, glPixelStorei:Df, glReadPixels:Gf, glRenderbufferStorage:Hf, glShaderSource:If, glStencilFunc:Jf, 
glStencilMask:Kf, glStencilOp:Lf, glStencilOpSeparate:Mf, glTexImage2D:Nf, glTexParameteri:Of, glTexSubImage2D:Pf, glUniform1i:Rf, glUniform2fv:Tf, glUniform4fv:Uf, glUseProgram:Vf, glVertexAttribPointer:Xf, glViewport:Yf}, Yg;
function Zg(a = []) {
  k(0 == Ya, 'cannot call main when async dependencies remain! (listen on Module["onRuntimeInitialized"])');
  k("undefined" === typeof Wa || 0 == Wa.length, "cannot call main when preRun functions remain to be called");
  var b = Wg;
  a.unshift(ja);
  var c = a.length, d = $d(4 * (c + 1)), e = d, f;
  for (f of a) {
    q[e >> 2] = ae(f), e += 4;
  }
  q[e >> 2] = 0;
  try {
    var g = b(c, d);
    wc(g, !0);
  } catch (l) {
    tc(l);
  }
}
function $g(a = ia) {
  function b() {
    k(!Yg);
    Yg = !0;
    h.calledRun = !0;
    if (!sa) {
      k(!Ha);
      Ha = !0;
      wa();
      if (!h.noFSInit && !E.Ga) {
        k(!E.Ga, "FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)");
        E.Ga = !0;
        d ??= h.stdin;
        e ??= h.stdout;
        c ??= h.stderr;
        d ? E.$("/dev", "stdin", d) : E.symlink("/dev/tty", "/dev/stdin");
        e ? E.$("/dev", "stdout", null, e) : E.symlink("/dev/tty", "/dev/stdout");
        c ? E.$("/dev", "stderr", null, c) : E.symlink("/dev/tty1", "/dev/stderr");
        var c = E.open("/dev/stdin", 0);
        var d = E.open("/dev/stdout", 1);
        var e = E.open("/dev/stderr", 1);
        k(0 === c.fd, `invalid handle for stdin (${c.fd})`);
        k(1 === d.fd, `invalid handle for stdout (${d.fd})`);
        k(2 === e.fd, `invalid handle for stderr (${e.fd})`);
      }
      Oa.__wasm_call_ctors();
      E.Bb = !1;
      wa();
      h.onRuntimeInitialized?.();
      Aa("onRuntimeInitialized");
      h.noInitialRun || Zg(a);
      wa();
      if (h.postRun) {
        for ("function" == typeof h.postRun && (h.postRun = [h.postRun]); h.postRun.length;) {
          c = h.postRun.shift(), Va.push(c);
        }
      }
      Aa("postRun");
      Ua(Va);
    }
  }
  if (0 < Ya) {
    Za = $g;
  } else {
    Xg();
    ua();
    if (h.preRun) {
      for ("function" == typeof h.preRun && (h.preRun = [h.preRun]); h.preRun.length;) {
        Xa();
      }
    }
    Aa("preRun");
    Ua(Wa);
    0 < Ya ? Za = $g : (h.setStatus ? (h.setStatus("Running..."), setTimeout(() => {
      setTimeout(() => h.setStatus(""), 1);
      b();
    }, 1)) : b(), wa());
  }
}
function vc() {
  var a = qa, b = m, c = !1;
  qa = m = () => {
    c = !0;
  };
  try {
    mc(0);
    for (var d of ["stdout", "stderr"]) {
      var e = gc("/dev/" + d);
      if (!e) {
        return;
      }
      xb[e.object.rdev]?.output?.length && (c = !0);
    }
  } catch (f) {
  }
  qa = a;
  m = b;
  c && Fa("stdio streams had content in them that was not flushed. you should set EXIT_RUNTIME to 1 (see the Emscripten FAQ), or make sure to emit a newline when you printf etc.");
}
var Oa;
(async function() {
  function a(d) {
    var e = Oa = d.exports;
    d = {};
    for (let [f, g] of Object.entries(e)) {
      "function" == typeof g ? (e = yg(g), d[f] = e) : d[f] = g;
    }
    d = Oa = d;
    k("undefined" != typeof d.__main_argc_argv, "missing Wasm export: __main_argc_argv");
    k("undefined" != typeof d.free, "missing Wasm export: free");
    k("undefined" != typeof d.malloc, "missing Wasm export: malloc");
    k("undefined" != typeof d.fflush, "missing Wasm export: fflush");
    k("undefined" != typeof d.web_on_file_ready, "missing Wasm export: web_on_file_ready");
    k("undefined" != typeof d.web_on_file_cancelled, "missing Wasm export: web_on_file_cancelled");
    k("undefined" != typeof d.strerror, "missing Wasm export: strerror");
    k("undefined" != typeof d.emscripten_stack_get_end, "missing Wasm export: emscripten_stack_get_end");
    k("undefined" != typeof d.emscripten_stack_get_base, "missing Wasm export: emscripten_stack_get_base");
    k("undefined" != typeof d.emscripten_stack_init, "missing Wasm export: emscripten_stack_init");
    k("undefined" != typeof d.emscripten_stack_get_free, "missing Wasm export: emscripten_stack_get_free");
    k("undefined" != typeof d._emscripten_stack_restore, "missing Wasm export: _emscripten_stack_restore");
    k("undefined" != typeof d._emscripten_stack_alloc, "missing Wasm export: _emscripten_stack_alloc");
    k("undefined" != typeof d.emscripten_stack_get_current, "missing Wasm export: emscripten_stack_get_current");
    k("undefined" != typeof d.dynCall_v, "missing Wasm export: dynCall_v");
    k("undefined" != typeof d.dynCall_ddd, "missing Wasm export: dynCall_ddd");
    k("undefined" != typeof d.dynCall_dd, "missing Wasm export: dynCall_dd");
    k("undefined" != typeof d.dynCall_d, "missing Wasm export: dynCall_d");
    k("undefined" != typeof d.dynCall_viii, "missing Wasm export: dynCall_viii");
    k("undefined" != typeof d.dynCall_iiiiii, "missing Wasm export: dynCall_iiiiii");
    k("undefined" != typeof d.dynCall_viiiiii, "missing Wasm export: dynCall_viiiiii");
    k("undefined" != typeof d.dynCall_iiii, "missing Wasm export: dynCall_iiii");
    k("undefined" != typeof d.dynCall_vii, "missing Wasm export: dynCall_vii");
    k("undefined" != typeof d.dynCall_ii, "missing Wasm export: dynCall_ii");
    k("undefined" != typeof d.dynCall_vi, "missing Wasm export: dynCall_vi");
    k("undefined" != typeof d.dynCall_viiiiiif, "missing Wasm export: dynCall_viiiiiif");
    k("undefined" != typeof d.dynCall_viiiiffii, "missing Wasm export: dynCall_viiiiffii");
    k("undefined" != typeof d.dynCall_viiiifiii, "missing Wasm export: dynCall_viiiifiii");
    k("undefined" != typeof d.dynCall_vifff, "missing Wasm export: dynCall_vifff");
    k("undefined" != typeof d.dynCall_iiiii, "missing Wasm export: dynCall_iiiii");
    k("undefined" != typeof d.dynCall_iiiiiiii, "missing Wasm export: dynCall_iiiiiiii");
    k("undefined" != typeof d.dynCall_iii, "missing Wasm export: dynCall_iii");
    k("undefined" != typeof d.dynCall_iiiiiii, "missing Wasm export: dynCall_iiiiiii");
    k("undefined" != typeof d.dynCall_viiii, "missing Wasm export: dynCall_viiii");
    k("undefined" != typeof d.dynCall_viiiii, "missing Wasm export: dynCall_viiiii");
    k("undefined" != typeof d.dynCall_ji, "missing Wasm export: dynCall_ji");
    k("undefined" != typeof d.dynCall_jiji, "missing Wasm export: dynCall_jiji");
    k("undefined" != typeof d.dynCall_i, "missing Wasm export: dynCall_i");
    k("undefined" != typeof d.dynCall_vffff, "missing Wasm export: dynCall_vffff");
    k("undefined" != typeof d.dynCall_vf, "missing Wasm export: dynCall_vf");
    k("undefined" != typeof d.dynCall_viiiiiiii, "missing Wasm export: dynCall_viiiiiiii");
    k("undefined" != typeof d.dynCall_viiiiiiiii, "missing Wasm export: dynCall_viiiiiiiii");
    k("undefined" != typeof d.dynCall_vff, "missing Wasm export: dynCall_vff");
    k("undefined" != typeof d.dynCall_viiiiiii, "missing Wasm export: dynCall_viiiiiii");
    k("undefined" != typeof d.dynCall_vfi, "missing Wasm export: dynCall_vfi");
    k("undefined" != typeof d.dynCall_viif, "missing Wasm export: dynCall_viif");
    k("undefined" != typeof d.dynCall_vif, "missing Wasm export: dynCall_vif");
    k("undefined" != typeof d.dynCall_viff, "missing Wasm export: dynCall_viff");
    k("undefined" != typeof d.dynCall_viffff, "missing Wasm export: dynCall_viffff");
    k("undefined" != typeof d.dynCall_vfff, "missing Wasm export: dynCall_vfff");
    k("undefined" != typeof d.dynCall_viiiiiiiiii, "missing Wasm export: dynCall_viiiiiiiiii");
    k("undefined" != typeof d.dynCall_viiiiiiiiiii, "missing Wasm export: dynCall_viiiiiiiiiii");
    k("undefined" != typeof d.dynCall_viifi, "missing Wasm export: dynCall_viifi");
    k("undefined" != typeof d.dynCall_iiij, "missing Wasm export: dynCall_iiij");
    k("undefined" != typeof d.dynCall_viij, "missing Wasm export: dynCall_viij");
    k("undefined" != typeof d.dynCall_iidiiii, "missing Wasm export: dynCall_iidiiii");
    k("undefined" != typeof d.asyncify_start_unwind, "missing Wasm export: asyncify_start_unwind");
    k("undefined" != typeof d.asyncify_stop_unwind, "missing Wasm export: asyncify_stop_unwind");
    k("undefined" != typeof d.asyncify_start_rewind, "missing Wasm export: asyncify_start_rewind");
    k("undefined" != typeof d.asyncify_stop_rewind, "missing Wasm export: asyncify_stop_rewind");
    k("undefined" != typeof d.memory, "missing Wasm export: memory");
    k("undefined" != typeof d.__indirect_function_table, "missing Wasm export: __indirect_function_table");
    Wg = h._main = B("__main_argc_argv", 2);
    Qg = B("free", 1);
    qd = B("malloc", 1);
    mc = B("fflush", 1);
    h._web_on_file_ready = B("web_on_file_ready", 1);
    h._web_on_file_cancelled = B("web_on_file_cancelled", 0);
    lc = B("strerror", 1);
    va = d.emscripten_stack_get_end;
    Xg = d.emscripten_stack_init;
    be = d._emscripten_stack_restore;
    $d = d._emscripten_stack_alloc;
    sc = d.emscripten_stack_get_current;
    dynCall_v = C.v = B("dynCall_v", 1);
    C.ddd = B("dynCall_ddd", 3);
    C.dd = B("dynCall_dd", 2);
    C.d = B("dynCall_d", 1);
    C.viii = B("dynCall_viii", 4);
    C.iiiiii = B("dynCall_iiiiii", 6);
    C.viiiiii = B("dynCall_viiiiii", 7);
    ge = C.iiii = B("dynCall_iiii", 4);
    dynCall_vii = C.vii = B("dynCall_vii", 3);
    C.ii = B("dynCall_ii", 2);
    dynCall_vi = C.vi = B("dynCall_vi", 2);
    C.viiiiiif = B("dynCall_viiiiiif", 8);
    C.viiiiffii = B("dynCall_viiiiffii", 9);
    C.viiiifiii = B("dynCall_viiiifiii", 9);
    C.vifff = B("dynCall_vifff", 5);
    C.iiiii = B("dynCall_iiiii", 5);
    C.iiiiiiii = B("dynCall_iiiiiiii", 8);
    dynCall_iii = C.iii = B("dynCall_iii", 3);
    C.iiiiiii = B("dynCall_iiiiiii", 7);
    C.viiii = B("dynCall_viiii", 5);
    C.viiiii = B("dynCall_viiiii", 6);
    C.ji = B("dynCall_ji", 2);
    C.jiji = B("dynCall_jiji", 4);
    C.i = B("dynCall_i", 1);
    C.vffff = B("dynCall_vffff", 5);
    C.vf = B("dynCall_vf", 2);
    C.viiiiiiii = B("dynCall_viiiiiiii", 9);
    C.viiiiiiiii = B("dynCall_viiiiiiiii", 10);
    C.vff = B("dynCall_vff", 3);
    C.viiiiiii = B("dynCall_viiiiiii", 8);
    C.vfi = B("dynCall_vfi", 3);
    C.viif = B("dynCall_viif", 4);
    C.vif = B("dynCall_vif", 3);
    C.viff = B("dynCall_viff", 4);
    C.viffff = B("dynCall_viffff", 6);
    C.vfff = B("dynCall_vfff", 4);
    C.viiiiiiiiii = B("dynCall_viiiiiiiiii", 11);
    C.viiiiiiiiiii = B("dynCall_viiiiiiiiiii", 12);
    C.viifi = B("dynCall_viifi", 5);
    C.iiij = B("dynCall_iiij", 4);
    C.viij = B("dynCall_viij", 4);
    C.iidiiii = B("dynCall_iidiiii", 7);
    Og = B("asyncify_start_unwind", 1);
    Bg = B("asyncify_stop_unwind", 0);
    Ng = B("asyncify_start_rewind", 1);
    Pg = B("asyncify_stop_rewind", 0);
    Ja = d.memory;
    Ia();
    bb("wasm-instantiate");
    return Oa;
  }
  cb("wasm-instantiate");
  var b = h;
  tg();
  var c = {env:ug, wasi_snapshot_preview1:ug};
  if (h.instantiateWasm) {
    return new Promise((d, e) => {
      try {
        h.instantiateWasm(c, (f, g) => {
          d(a(f, g));
        });
      } catch (f) {
        m(`Module.instantiateWasm callback failed with error: ${f}`), e(f);
      }
    });
  }
  Pa ??= h.locateFile ? h.locateFile("aslan.wasm", ma) : ma + "aslan.wasm";
  return function(d) {
    k(h === b, "the Module object should not be replaced during async compilation - perhaps the order of HTML elements is wrong?");
    b = null;
    return a(d.instance);
  }(await Sa(c));
})();
$g();

