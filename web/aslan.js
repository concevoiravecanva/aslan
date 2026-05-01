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
var ba = !!globalThis.window, ca = !!globalThis.WorkerGlobalScope, ea = globalThis.process?.versions?.node && "renderer" != globalThis.process?.type, fa = !ba && !ea && !ca;
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
          var y = await fetch(r);
        } catch (S) {
          throw Error(`Network Error: ${r}`, {e:S});
        }
        if (!y.ok) {
          throw Error(`${y.status}: ${y.url}`);
        }
        v = [];
        t = Number(y.headers.get("Content-Length") || t);
        let I = 0;
        h.setStatus && h.setStatus("Downloading data...");
        for (y = y.body.getReader();;) {
          var {done:Q, value:R} = await y.read();
          if (Q) {
            break;
          }
          v.push(R);
          I += R.length;
          h.dataFileDownloads[r] = {loaded:I, total:t};
          let S = 0, da = 0;
          for (var aa of Object.values(h.dataFileDownloads)) {
            S += aa.loaded, da += aa.total;
          }
          h.setStatus && h.setStatus(`Downloading data... (${S}/${da})`);
        }
        r = new Uint8Array(v.map(S => S.length).reduce((S, da) => S + da, 0));
        aa = 0;
        for (const S of v) {
          r.set(S, aa), aa += S.length;
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
        r.preloadResults["web/aslan.data"] = {Qc:!1};
        p ||= await m;
        (async function(v) {
          if (!v) {
            throw Error("Loading data file failed.");
          }
          if (v.constructor.name !== ArrayBuffer.name) {
            throw Error("bad input to processPackageData " + v.constructor.name);
          }
          v = new Uint8Array(v);
          for (var y of c.files) {
            var I = y.filename;
            r.FS_createDataFile(I, null, v.subarray(y.start, y.end), !0, !0, !0);
            r.removeRunDependency(`fp ${I}`);
          }
          r.removeRunDependency("datafile_web/aslan.data");
        })(p);
      }
      "object" === typeof window ? window.encodeURIComponent(window.location.pathname.substring(0, window.location.pathname.lastIndexOf("/")) + "/") : "undefined" === typeof process && "undefined" !== typeof location && encodeURIComponent(location.pathname.substring(0, location.pathname.lastIndexOf("/")) + "/");
      var f = h.locateFile ? h.locateFile("aslan.data", "") : "aslan.data", g = c.remote_package_size, m, p = h.getPreloadedPackage && h.getPreloadedPackage(f, g);
      p || (m = d(f, g));
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
if (ea) {
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
  if (ba || ca) {
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
var qa = console.log.bind(console), l = console.error.bind(console);
k(!fa, "shell environment detected but not enabled at build time.  Add `shell` to `-sENVIRONMENT` to enable.");
var ra;
globalThis.WebAssembly || l("no native wasm support detected");
var sa = !1, ta;
function k(a, b) {
  a || n("Assertion failed" + (b ? ": " + b : ""));
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
    34821223 == b && 2310721022 == c || n(`Stack overflow! Stack cookie has been overwritten at ${xa(a)}, expected hex dwords 0x89BACDFE and 0x2135467, but received ${xa(c)} ${xa(b)}`);
    1668509029 != q[0] && n("Runtime error: The application has corrupted its heap memory area (address zero)!");
  }
}
var ya = new Int16Array(1), za = new Int8Array(ya.buffer);
ya[0] = 25459;
115 === za[0] && 99 === za[1] || n("Runtime error: expected the system to be little-endian! (Run with -sSUPPORT_BIG_ENDIAN to bypass)");
function Aa(a) {
  Object.getOwnPropertyDescriptor(h, a) || Object.defineProperty(h, a, {configurable:!0, set() {
    n(`Attempt to set \`Module.${a}\` after it has already been processed.  This can happen, for example, when code is injected via '--post-js' rather than '--pre-js'`);
  }});
}
function u(a) {
  return () => k(!1, `call to '${a}' via reference taken before Wasm module initialization`);
}
function Ba(a) {
  Object.getOwnPropertyDescriptor(h, a) && n(`\`Module.${a}\` was supplied but \`${a}\` not included in INCOMING_MODULE_JS_API`);
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
    n(b);
  }});
}
var Ha = !1;
function Ia() {
  var a = Ja.buffer;
  w = new Int8Array(a);
  Ka = new Int16Array(a);
  x = new Uint8Array(a);
  La = new Uint16Array(a);
  z = new Int32Array(a);
  q = new Uint32Array(a);
  A = new Float32Array(a);
  Ma = new Float64Array(a);
  Na = new BigInt64Array(a);
  new BigUint64Array(a);
}
k(globalThis.Int32Array && globalThis.Float64Array && Int32Array.prototype.subarray && Int32Array.prototype.set, "JS engine does not provide full typed array support");
function n(a) {
  h.onAbort?.(a);
  a = `Aborted(${a})`;
  l(a);
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
    l(`failed to asynchronously prepare wasm: ${d}`), pa(a) && l(`warning: Loading from a file URI (${a}) is not supported in most browsers. See https://emscripten.org/docs/getting_started/FAQ.html#how-do-i-run-a-local-webserver-for-testing-why-does-my-program-stall-in-downloading-or-preparing`), n(d);
  }
}
async function Sa(a) {
  var b = Pa;
  if (!ra && !pa(b) && !ea) {
    try {
      var c = fetch(b, {credentials:"same-origin"});
      return await WebAssembly.instantiateStreaming(c, a);
    } catch (d) {
      l(`wasm streaming compile failed: ${d}`), l("falling back to ArrayBuffer instantiation");
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
var Ka, z, Na, w, A, Ma, La, q, x, Ua = a => {
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
        b || (b = !0, l("still waiting on run dependencies:")), l(`dependency: ${c}`);
      }
      b && l("(end of list)");
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
      w[a] = b;
      break;
    case "i8":
      w[a] = b;
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
      n(`invalid type for setValue: ${c}`);
  }
}
var Fa = a => {
  Fa.cb || (Fa.cb = {});
  Fa.cb[a] || (Fa.cb[a] = 1, ea && (a = "warning: " + a), l(a));
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
  return a ? hb(x, a, b, c) : "";
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
  if (ea) {
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
    for (var m = 0; m < g.length && "" === g[m]; m++) {
    }
    for (var p = g.length - 1; 0 <= p && "" === g[p]; p--) {
    }
    return m > p ? [] : g.slice(m, p - m + 1);
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
  if (!a.tty || !a.tty.Z.tb) {
    throw new E.g(60);
  }
  for (var e = 0, f = 0; f < d; f++) {
    try {
      var g = a.tty.Z.tb(a.tty);
    } catch (m) {
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
  if (!a.tty || !a.tty.Z.ab) {
    throw new E.g(60);
  }
  try {
    for (var e = 0; e < d; e++) {
      a.tty.Z.ab(a.tty, b[c + e]);
    }
  } catch (f) {
    throw new E.g(29);
  }
  d && (a.node.mtime = a.node.ctime = Date.now());
  return e;
}}, Bb = {tb() {
  a: {
    if (!tb.length) {
      var a = null;
      if (ea) {
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
}, ab(a, b) {
  null === b || 10 === b ? (qa(hb(a.output)), a.output = []) : 0 != b && a.output.push(b);
}, fsync(a) {
  0 < a.output?.length && (qa(hb(a.output)), a.output = []);
}, dc() {
  return {Cc:25856, Ec:5, Bc:191, Dc:35387, Ac:[3, 28, 127, 21, 4, 0, 1, 0, 17, 19, 26, 0, 18, 15, 23, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]};
}, ec() {
  return 0;
}, fc() {
  return [24, 80];
}}, Cb = {ab(a, b) {
  null === b || 10 === b ? (l(hb(a.output)), a.output = []) : 0 != b && a.output.push(b);
}, fsync(a) {
  0 < a.output?.length && (l(hb(a.output)), a.output = []);
}}, Db = () => {
  n("internal error: mmapAlloc called but `emscripten_builtin_memalign` native symbol not exported");
}, F = {T:null, o() {
  return F.createNode(null, "/", 16895, 0);
}, createNode(a, b, c, d) {
  if (24576 === (c & 61440) || E.isFIFO(c)) {
    throw new E.g(63);
  }
  F.T || (F.T = {dir:{node:{L:F.h.L, P:F.h.P, lookup:F.h.lookup, X:F.h.X, rename:F.h.rename, unlink:F.h.unlink, rmdir:F.h.rmdir, readdir:F.h.readdir, symlink:F.h.symlink}, stream:{D:F.j.D}}, file:{node:{L:F.h.L, P:F.h.P}, stream:{D:F.j.D, read:F.j.read, write:F.j.write, la:F.j.la, sa:F.j.sa}}, link:{node:{L:F.h.L, P:F.h.P, readlink:F.h.readlink}, stream:{}}, ob:{node:{L:F.h.L, P:F.h.P}, stream:E.Kb}});
  c = E.createNode(a, b, c, d);
  G(c.mode) ? (c.h = F.T.dir.node, c.j = F.T.dir.stream, c.l = {}) : E.isFile(c.mode) ? (c.h = F.T.file.node, c.j = F.T.file.stream, c.A = 0, c.l = F.Sb ?? (F.Sb = new Uint8Array(0))) : 40960 === (c.mode & 61440) ? (c.h = F.T.link.node, c.j = F.T.link.stream) : 8192 === (c.mode & 61440) && (c.h = F.T.ob.node, c.j = F.T.ob.stream);
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
}, P(a, b) {
  for (var c of ["mode", "atime", "mtime", "ctime"]) {
    null != b[c] && (a[c] = b[c]);
  }
  void 0 !== b.size && (b = b.size, a.A != b && (c = a.l, a.l = new Uint8Array(b), a.l.set(c.subarray(0, Math.min(b, a.A))), a.A = b));
}, lookup() {
  throw new E.g(44);
}, X(a, b, c, d) {
  return F.createNode(a, b, c, d);
}, rename(a, b, c) {
  try {
    var d = Eb(b, c);
  } catch (f) {
  }
  if (d) {
    if (G(a.mode)) {
      for (var e in d.l) {
        throw new E.g(55);
      }
    }
    Fb(d);
  }
  delete a.parent.l[a.name];
  b.l[c] = a;
  a.name = c;
  b.ctime = b.mtime = a.parent.ctime = a.parent.mtime = Date.now();
}, unlink(a, b) {
  delete a.l[b];
  a.ctime = a.mtime = Date.now();
}, rmdir(a, b) {
  var c = Eb(a, b), d;
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
  b.buffer === w.buffer && (f = !1);
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
    g >= f || (f = Math.max(f, g * (1048576 > g ? 2.0 : 1.125) >>> 0), g && (f = Math.max(f, 256)), k(E.isFile(a.mode), "getFileDataAsTypedArray called on non-file"), g = a.l.subarray(0, a.A), a.l = new Uint8Array(f), a.l.set(g));
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
}, la(a, b, c, d, e) {
  if (!E.isFile(a.node.mode)) {
    throw new E.g(43);
  }
  a = a.node.l;
  if (e & 2 || a.buffer !== w.buffer) {
    d = !0;
    e = Db();
    if (!e) {
      throw new E.g(48);
    }
    if (a) {
      if (0 < c || c + b < a.length) {
        a.subarray ? a = a.subarray(c, c + b) : a = Array.prototype.slice.call(a, c, c + b);
      }
      w.set(a, e);
    }
  } else {
    d = !1, e = a.byteOffset;
  }
  return {ma:e, Jb:d};
}, sa(a, b, c, d) {
  F.j.write(a, b, 0, d, c, !1);
  return 0;
}}}, Gb = a => {
  "string" == typeof a && (a = wb(a));
  a.subarray || (a = new Uint8Array(a));
  return a;
}, Hb = (a, b) => {
  var c = 0;
  a && (c |= 365);
  b && (c |= 146);
  return c;
}, Ib = {EPERM:63, ENOENT:44, ESRCH:71, EINTR:27, EIO:29, ENXIO:60, E2BIG:1, ENOEXEC:45, EBADF:8, ECHILD:12, EAGAIN:6, EWOULDBLOCK:6, ENOMEM:48, EACCES:2, EFAULT:21, ENOTBLK:105, EBUSY:10, EEXIST:20, EXDEV:75, ENODEV:43, ENOTDIR:54, EISDIR:31, EINVAL:28, ENFILE:41, EMFILE:33, ENOTTY:59, ETXTBSY:74, EFBIG:22, ENOSPC:51, ESPIPE:70, EROFS:69, EMLINK:34, EPIPE:64, EDOM:18, ERANGE:68, ENOMSG:49, EIDRM:24, ECHRNG:106, EL2NSYNC:156, EL3HLT:107, EL3RST:108, ELNRNG:109, EUNATCH:110, ENOCSI:111, EL2HLT:112, 
EDEADLK:16, ENOLCK:46, EBADE:113, EBADR:114, EXFULL:115, ENOANO:104, EBADRQC:103, EBADSLT:102, EDEADLOCK:16, EBFONT:101, ENOSTR:100, ENODATA:116, ETIME:117, ENOSR:118, ENONET:119, ENOPKG:120, EREMOTE:121, ENOLINK:47, EADV:122, ESRMNT:123, ECOMM:124, EPROTO:65, EMULTIHOP:36, EDOTDOT:125, EBADMSG:9, ENOTUNIQ:126, EBADFD:127, EREMCHG:128, ELIBACC:129, ELIBBAD:130, ELIBSCN:131, ELIBMAX:132, ELIBEXEC:133, ENOSYS:52, ENOTEMPTY:55, ENAMETOOLONG:37, ELOOP:32, EOPNOTSUPP:138, EPFNOSUPPORT:139, ECONNRESET:15, 
ENOBUFS:42, EAFNOSUPPORT:5, EPROTOTYPE:67, ENOTSOCK:57, ENOPROTOOPT:50, ESHUTDOWN:140, ECONNREFUSED:14, EADDRINUSE:3, ECONNABORTED:13, ENETUNREACH:40, ENETDOWN:38, ETIMEDOUT:73, EHOSTDOWN:142, EHOSTUNREACH:23, EINPROGRESS:26, EALREADY:7, EDESTADDRREQ:17, EMSGSIZE:35, EPROTONOSUPPORT:66, ESOCKTNOSUPPORT:137, EADDRNOTAVAIL:4, ENETRESET:39, EISCONN:30, ENOTCONN:53, ETOOMANYREFS:141, EUSERS:136, EDQUOT:19, ESTALE:72, ENOTSUP:138, ENOMEDIUM:148, EILSEQ:25, EOVERFLOW:61, ECANCELED:11, ENOTRECOVERABLE:56, 
EOWNERDEAD:62, ESTRPIPE:135}, Jb = async a => {
  var b = await na(a);
  k(b, `Loading data file "${a}" failed (no arrayBuffer).`);
  return new Uint8Array(b);
}, Kb = [], Nb = async(a, b) => {
  "undefined" != typeof Lb && Mb();
  for (var c of Kb) {
    if (c.canHandle(b)) {
      return k("AsyncFunction" === c.handle.constructor.name, "Filesystem plugin handlers must be async functions (See #24914)"), c.handle(a, b);
    }
  }
  return a;
}, Pb = async(a, b, c, d, e, f, g, m) => {
  var p = b ? rb(lb(a + "/" + b)) : a, r;
  a: {
    for (var t = r = `cp ${p}`;;) {
      if (!$a[r]) {
        break a;
      }
      r = t + Math.random();
    }
  }
  cb(r);
  try {
    t = c, "string" == typeof c && (t = await Jb(c)), t = await Nb(t, p), m?.(), f || Ob(a, b, t, d, e, g);
  } finally {
    bb(r);
  }
};
function zb(a, b) {
  E.qb[a] = {j:b};
}
function G(a) {
  return 16384 === (a & 61440);
}
function Eb(a, b) {
  var c = G(a.mode) ? (c = Qb(a, "x")) ? c : a.h.lookup ? 0 : 2 : 54;
  if (c) {
    throw new E.g(c);
  }
  for (c = E.Y[Rb(a.id, b)]; c; c = c.fa) {
    var d = c.name;
    if (c.parent.id === a.id && d === b) {
      return c;
    }
  }
  return E.lookup(a, b);
}
function Fb(a) {
  var b = Rb(a.parent.id, a.name);
  if (E.Y[b] === a) {
    E.Y[b] = a.fa;
  } else {
    for (b = E.Y[b]; b;) {
      if (b.fa === a) {
        b.fa = a.fa;
        break;
      }
      b = b.fa;
    }
  }
}
function Ob(a, b, c, d, e, f) {
  var g = b;
  a && (a = "string" == typeof a ? a : Sb(a), g = b ? lb(a + "/" + b) : a);
  a = Hb(d, e);
  g = E.create(g, a);
  c && (c = Gb(c), E.chmod(g, a | 146), b = E.open(g, 577), E.write(b, c, 0, c.length, 0, f), E.close(b), E.chmod(g, a));
}
function H(a, b = {}) {
  if (!a) {
    throw new E.g(44);
  }
  b.Aa ?? (b.Aa = !0);
  "/" === a.charAt(0) || (a = E.cwd() + "/" + a);
  var c = 0;
  a: for (; 40 > c; c++) {
    a = a.split("/").filter(m => !!m);
    for (var d = E.root, e = "/", f = 0; f < a.length; f++) {
      var g = f === a.length - 1;
      if (g && b.parent) {
        break;
      }
      if ("." !== a[f]) {
        if (".." === a[f]) {
          if (e = mb(e), E.qa(d)) {
            a = e + "/" + a.slice(f + 1).join("/");
            c--;
            continue a;
          } else {
            d = d.parent;
          }
        } else {
          e = lb(e + "/" + a[f]);
          try {
            d = Eb(d, a[f]);
          } catch (m) {
            if (44 === m?.s && g && b.kc) {
              return {path:e};
            }
            throw m;
          }
          !d.N || g && !b.Aa || (d = d.N.root);
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
function Sb(a) {
  for (var b;;) {
    if (E.qa(a)) {
      return a = a.o.xb, b ? "/" !== a[a.length - 1] ? `${a}/${b}` : a + b : a;
    }
    b = b ? `${a.name}/${b}` : a.name;
    a = a.parent;
  }
}
function Rb(a, b) {
  for (var c = 0, d = 0; d < b.length; d++) {
    c = (c << 5) - c + b.charCodeAt(d) | 0;
  }
  return (a + c >>> 0) % E.Y.length;
}
function Tb(a) {
  var b = Rb(a.parent.id, a.name);
  a.fa = E.Y[b];
  E.Y[b] = a;
}
function Qb(a, b) {
  return E.ub ? 0 : b.includes("r") && !(a.mode & 292) || b.includes("w") && !(a.mode & 146) || b.includes("x") && !(a.mode & 73) ? 2 : 0;
}
function Ub(a, b) {
  if (!G(a.mode)) {
    return 54;
  }
  try {
    return Eb(a, b), 20;
  } catch (c) {
  }
  return Qb(a, "wx");
}
function Vb(a, b, c) {
  try {
    var d = Eb(a, b);
  } catch (e) {
    return e.s;
  }
  if (a = Qb(a, "wx")) {
    return a;
  }
  if (c) {
    if (!G(d.mode)) {
      return 54;
    }
    if (E.qa(d) || Sb(d) === E.cwd()) {
      return 10;
    }
  } else if (G(d.mode)) {
    return 31;
  }
  return 0;
}
function Wb(a, b) {
  if (!a) {
    throw new E.g(b);
  }
  return a;
}
function Xb(a) {
  a = E.$b(a);
  if (!a) {
    throw new E.g(8);
  }
  return a;
}
function Yb(a, b = -1) {
  k(-1 <= b);
  a = Object.assign(new E.Hb(), a);
  if (-1 == b) {
    a: {
      for (b = 0; b <= E.Ib; b++) {
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
function Zb(a, b = -1) {
  a = Yb(a, b);
  a.j?.Mc?.(a);
  return a;
}
function $b(a, b, c) {
  var d = a?.j.P;
  a = d ? a : b;
  d ??= b.h.P;
  Wb(d, 63);
  d(a, c);
}
function ac(a) {
  var b = [];
  for (a = [a]; a.length;) {
    var c = a.pop();
    b.push(c);
    a.push(...c.ra);
  }
  return b;
}
function bc(a) {
  var b = {zc:4096, Rc:4096, blocks:1e6, yc:5e5, xc:5e5, files:E.Ya, Oc:E.Ya - 1, Sc:42, flags:2, ad:255};
  a.h.Cb && Object.assign(b, a.h.Cb(a.o.lc.root));
  return b;
}
function cc(a, b, c) {
  "undefined" == typeof c && (c = b, b = 438);
  return E.X(a, b | 8192, c);
}
function dc(a, b, c, d) {
  $b(a, b, {mode:c & 4095 | b.mode & -4096, ctime:Date.now(), rb:d});
}
function ec(a, b, c) {
  if (G(b.mode)) {
    throw new E.g(31);
  }
  if (!E.isFile(b.mode)) {
    throw new E.g(28);
  }
  var d = Qb(b, "w");
  if (d) {
    throw new E.g(d);
  }
  $b(a, b, {size:c, timestamp:Date.now()});
}
function fc(a, b) {
  try {
    var c = H(a, {K:!b});
    a = c.path;
  } catch (e) {
  }
  var d = {qa:!1, exists:!1, error:0, name:null, path:null, object:null, mc:!1, oc:null, nc:null};
  try {
    c = H(a, {parent:!0}), d.mc = !0, d.oc = c.path, d.nc = c.node, d.name = nb(a), c = H(a, {K:!b}), d.exists = !0, d.path = c.path, d.object = c.node, d.name = c.node.name, d.qa = "/" === c.path;
  } catch (e) {
    d.error = e.s;
  }
  return d;
}
function gc(a, b) {
  a = "string" == typeof a ? a : Sb(a);
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
function hc(a, b, c, d) {
  a = "string" == typeof a ? a : Sb(a);
  b = lb(a + "/" + b);
  return E.create(b, Hb(c, d));
}
function ic(a) {
  if (!(a.ic || a.jc || a.link || a.l)) {
    if (globalThis.XMLHttpRequest) {
      n("Lazy loading should have been performed (contents set) in createLazyFile, but it was not. Lazy loading only works in web workers. Use --embed-file or --preload-file in emcc on the main thread.");
    } else {
      try {
        a.l = oa(a.url);
      } catch (b) {
        throw new E.g(29);
      }
    }
  }
}
function jc(a, b, c, d, e) {
  function f(t, v, y, I, Q) {
    t = t.node.l;
    if (Q >= t.length) {
      return 0;
    }
    I = Math.min(t.length - Q, I);
    k(0 <= I);
    if (t.slice) {
      for (var R = 0; R < I; R++) {
        v[y + R] = t[Q + R];
      }
    } else {
      for (R = 0; R < I; R++) {
        v[y + R] = t.get(Q + R);
      }
    }
    return I;
  }
  class g {
    Ta=!1;
    G=[];
    Ra=void 0;
    yb=0;
    wb=0;
    get(t) {
      if (!(t > this.length - 1 || 0 > t)) {
        var v = t % this.chunkSize;
        return this.Ra(t / this.chunkSize | 0)[v];
      }
    }
    sc(t) {
      this.Ra = t;
    }
    Bb() {
      var t = new XMLHttpRequest();
      t.open("HEAD", c, !1);
      t.send(null);
      200 <= t.status && 300 > t.status || 304 === t.status || n("Couldn't load " + c + ". Status: " + t.status);
      var v = Number(t.getResponseHeader("Content-length")), y, I = (y = t.getResponseHeader("Accept-Ranges")) && "bytes" === y;
      t = (y = t.getResponseHeader("Content-Encoding")) && "gzip" === y;
      var Q = 1048576;
      I || (Q = v);
      var R = this;
      R.sc(aa => {
        var S = aa * Q, da = (aa + 1) * Q - 1;
        da = Math.min(da, v - 1);
        if ("undefined" == typeof R.G[aa]) {
          var yc = R.G;
          S > da && n("invalid range (" + S + ", " + da + ") or no bytes requested!");
          da > v - 1 && n("only " + v + " bytes available! programmer error!");
          var V = new XMLHttpRequest();
          V.open("GET", c, !1);
          v !== Q && V.setRequestHeader("Range", "bytes=" + S + "-" + da);
          V.responseType = "arraybuffer";
          V.overrideMimeType && V.overrideMimeType("text/plain; charset=x-user-defined");
          V.send(null);
          200 <= V.status && 300 > V.status || 304 === V.status || n("Couldn't load " + c + ". Status: " + V.status);
          S = void 0 !== V.response ? new Uint8Array(V.response || []) : wb(V.responseText || "");
          yc[aa] = S;
        }
        "undefined" == typeof R.G[aa] && n("doXHR failed!");
        return R.G[aa];
      });
      if (t || !v) {
        Q = v = 1, Q = v = this.Ra(0).length, qa("LazyFiles on gzip forces download of the whole file when length is accessed");
      }
      this.yb = v;
      this.wb = Q;
      this.Ta = !0;
    }
    get length() {
      this.Ta || this.Bb();
      return this.yb;
    }
    get chunkSize() {
      this.Ta || this.Bb();
      return this.wb;
    }
  }
  if (globalThis.XMLHttpRequest) {
    ca || n("Cannot do synchronous binary XHRs outside webworkers in modern browsers. Use --embed-file or --preload-file in emcc");
    var m = new g();
    var p = void 0;
  } else {
    p = c, m = void 0;
  }
  var r = hc(a, b, d, e);
  m ? r.l = m : p && (r.l = null, r.url = p);
  Object.defineProperties(r, {A:{get:function() {
    return this.l.length;
  }}});
  a = {};
  for (const [t, v] of Object.entries(r.j)) {
    a[t] = (...y) => {
      ic(r);
      return v(...y);
    };
  }
  a.read = (t, v, y, I, Q) => {
    ic(r);
    return f(t, v, y, I, Q);
  };
  a.la = (t, v, y) => {
    ic(r);
    var I = Db();
    if (!I) {
      throw new E.g(48);
    }
    f(t, w, I, v, y);
    return {ma:I, Jb:!0};
  };
  r.j = a;
  return r;
}
var E = {root:null, ra:[], qb:{}, streams:[], Ya:1, Y:null, pb:"/", Ba:!1, ub:!0, Xb:null, ta:0, g:class extends Error {
  name="ErrnoError";
  constructor(a) {
    super(Ha ? D(kc(a)) : "");
    this.s = a;
    for (var b in Ib) {
      if (Ib[b] === a) {
        this.code = b;
        break;
      }
    }
  }
}, Hb:class {
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
}, Gb:class {
  h={};
  j={};
  N=null;
  constructor(a, b, c, d) {
    a ||= this;
    this.parent = a;
    this.o = a.o;
    this.id = E.Ya++;
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
  get jc() {
    return G(this.mode);
  }
  get ic() {
    return 8192 === (this.mode & 61440);
  }
}, createNode(a, b, c, d) {
  k("object" == typeof a);
  a = new E.Gb(a, b, c, d);
  Tb(a);
  return a;
}, qa(a) {
  return a === a.parent;
}, isFile(a) {
  return 32768 === (a & 61440);
}, isFIFO(a) {
  return 4096 === (a & 61440);
}, isSocket(a) {
  return 49152 === (a & 49152);
}, Ib:4096, $b:a => E.streams[a], Kb:{open(a) {
  a.j = E.Zb(a.node.rdev).j;
  a.j.open?.(a);
}, D() {
  throw new E.g(70);
}}, Ua:a => a >> 8, Xc:a => a & 255, ea:(a, b) => a << 8 | b, Zb:a => E.qb[a], Db(a, b) {
  function c(m) {
    k(0 < E.ta);
    E.ta--;
    return b(m);
  }
  function d(m) {
    if (m) {
      if (!d.Ub) {
        return d.Ub = !0, c(m);
      }
    } else {
      ++f >= e.length && c(null);
    }
  }
  "function" == typeof a && (b = a, a = !1);
  E.ta++;
  1 < E.ta && l(`warning: ${E.ta} FS.syncfs operations in flight at once, probably just doing extra work`);
  var e = ac(E.root.o), f = 0, g;
  for (g of e) {
    g.type.Db ? g.type.Db(g, a, d) : d(null);
  }
}, o(a, b, c) {
  if ("string" == typeof a) {
    throw a;
  }
  var d = "/" === c, e = !c;
  if (d && E.root) {
    throw new E.g(10);
  }
  if (!d && !e) {
    var f = H(c, {Aa:!1});
    c = f.path;
    f = f.node;
    if (f.N) {
      throw new E.g(10);
    }
    if (!G(f.mode)) {
      throw new E.g(54);
    }
  }
  b = {type:a, lc:b, xb:c, ra:[]};
  a = a.o(b);
  a.o = b;
  b.root = a;
  d ? E.root = a : f && (f.N = b, f.o && f.o.ra.push(b));
  return a;
}, ld(a) {
  a = H(a, {Aa:!1});
  if (!a.node.N) {
    throw new E.g(28);
  }
  a = a.node;
  var b = a.N, c = ac(b);
  for ([, e] of Object.entries(E.Y)) {
    for (; e;) {
      var d = e.fa;
      c.includes(e.o) && Fb(e);
      var e = d;
    }
  }
  a.N = null;
  e = a.o.ra.indexOf(b);
  k(-1 !== e);
  a.o.ra.splice(e, 1);
}, lookup(a, b) {
  return a.h.lookup(a, b);
}, X(a, b, c) {
  var d = H(a, {parent:!0}).node;
  a = nb(a);
  if (!a) {
    throw new E.g(28);
  }
  if ("." === a || ".." === a) {
    throw new E.g(20);
  }
  var e = Ub(d, a);
  if (e) {
    throw new E.g(e);
  }
  if (!d.h.X) {
    throw new E.g(63);
  }
  return d.h.X(d, a, b, c);
}, Cb(a) {
  return bc(H(a, {K:!0}).node);
}, jd(a) {
  return bc(a.node);
}, create(a, b = 438) {
  return E.X(a, b & 4095 | 32768, 0);
}, mkdir(a, b = 511) {
  return E.X(a, b & 1023 | 16384, 0);
}, Zc(a, b) {
  var c = a.split("/"), d = "", e;
  for (e of c) {
    if (e) {
      if (d || "/" === a.charAt(0)) {
        d += "/";
      }
      d += e;
      try {
        E.mkdir(d, b);
      } catch (f) {
        if (20 != f.s) {
          throw f;
        }
      }
    }
  }
}, symlink(a, b) {
  if (!rb(a)) {
    throw new E.g(44);
  }
  var c = H(b, {parent:!0}).node;
  if (!c) {
    throw new E.g(44);
  }
  b = nb(b);
  var d = Ub(c, b);
  if (d) {
    throw new E.g(d);
  }
  if (!c.h.symlink) {
    throw new E.g(63);
  }
  return c.h.symlink(c, b, a);
}, rename(a, b) {
  var c = mb(a), d = mb(b), e = nb(a), f = nb(b);
  var g = H(a, {parent:!0});
  var m = g.node;
  g = H(b, {parent:!0});
  g = g.node;
  if (!m || !g) {
    throw new E.g(44);
  }
  if (m.o !== g.o) {
    throw new E.g(75);
  }
  var p = Eb(m, e);
  a = sb(a, d);
  if ("." !== a.charAt(0)) {
    throw new E.g(28);
  }
  a = sb(b, c);
  if ("." !== a.charAt(0)) {
    throw new E.g(55);
  }
  try {
    var r = Eb(g, f);
  } catch (t) {
  }
  if (p !== r) {
    b = G(p.mode);
    if (e = Vb(m, e, b)) {
      throw new E.g(e);
    }
    if (e = r ? Vb(g, f, b) : Ub(g, f)) {
      throw new E.g(e);
    }
    if (!m.h.rename) {
      throw new E.g(63);
    }
    if (p.N || r && r.N) {
      throw new E.g(10);
    }
    if (g !== m && (e = Qb(m, "w"))) {
      throw new E.g(e);
    }
    Fb(p);
    try {
      m.h.rename(p, g, f), p.parent = g;
    } catch (t) {
      throw t;
    } finally {
      Tb(p);
    }
  }
}, rmdir(a) {
  var b = H(a, {parent:!0}).node;
  a = nb(a);
  var c = Eb(b, a), d = Vb(b, a, !0);
  if (d) {
    throw new E.g(d);
  }
  if (!b.h.rmdir) {
    throw new E.g(63);
  }
  if (c.N) {
    throw new E.g(10);
  }
  b.h.rmdir(b, a);
  Fb(c);
}, readdir(a) {
  a = H(a, {K:!0}).node;
  return Wb(a.h.readdir, 54)(a);
}, unlink(a) {
  var b = H(a, {parent:!0}).node;
  if (!b) {
    throw new E.g(44);
  }
  a = nb(a);
  var c = Eb(b, a), d = Vb(b, a, !1);
  if (d) {
    throw new E.g(d);
  }
  if (!b.h.unlink) {
    throw new E.g(63);
  }
  if (c.N) {
    throw new E.g(10);
  }
  b.h.unlink(b, a);
  Fb(c);
}, readlink(a) {
  a = H(a).node;
  if (!a) {
    throw new E.g(44);
  }
  if (!a.h.readlink) {
    throw new E.g(28);
  }
  return a.h.readlink(a);
}, stat(a, b) {
  a = H(a, {K:!b}).node;
  return Wb(a.h.L, 63)(a);
}, fstat(a) {
  var b = Xb(a);
  a = b.node;
  var c = b.j.L;
  b = c ? b : a;
  c ??= a.h.L;
  Wb(c, 63);
  return c(b);
}, lstat(a) {
  return E.stat(a, !0);
}, chmod(a, b, c) {
  a = "string" == typeof a ? H(a, {K:!c}).node : a;
  dc(null, a, b, c);
}, lchmod(a, b) {
  E.chmod(a, b, !0);
}, fchmod(a, b) {
  a = Xb(a);
  dc(a, a.node, b, !1);
}, chown(a, b, c, d) {
  a = "string" == typeof a ? H(a, {K:!d}).node : a;
  $b(null, a, {timestamp:Date.now(), rb:d});
}, lchown(a, b, c) {
  E.chown(a, b, c, !0);
}, fchown(a) {
  a = Xb(a);
  $b(a, a.node, {timestamp:Date.now(), rb:!1});
}, truncate(a, b) {
  if (0 > b) {
    throw new E.g(28);
  }
  a = "string" == typeof a ? H(a, {K:!0}).node : a;
  ec(null, a, b);
}, Tc(a, b) {
  a = Xb(a);
  if (0 > b || 0 === (a.flags & 2097155)) {
    throw new E.g(28);
  }
  ec(a, a.node, b);
}, md(a, b, c) {
  a = H(a, {K:!0}).node;
  Wb(a.h.P, 63)(a, {atime:b, mtime:c});
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
    var f = H(a, {K:!(b & 131072), kc:!0});
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
      d = E.X(a, c | 511, 0);
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
  if (!f && (d ? 40960 === (d.mode & 61440) ? e = 32 : (e = ["r", "w", "rw"][b & 3], b & 512 && (e += "w"), e = G(d.mode) && ("r" !== e || b & 576) ? 31 : Qb(d, e)) : e = 44, e)) {
    throw new E.g(e);
  }
  b & 512 && !f && E.truncate(d, 0);
  b &= -131713;
  b = Yb({node:d, path:Sb(d), flags:b, seekable:!0, position:0, j:d.j, tc:[], error:!1});
  b.j.open && b.j.open(b);
  f && E.chmod(d, c & 511);
  return b;
}, close(a) {
  if (null === a.fd) {
    throw new E.g(8);
  }
  a.Oa && (a.Oa = null);
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
  a.tc = [];
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
}, la(a, b, c, d, e) {
  if (0 !== (d & 2) && 0 === (e & 2) && 2 !== (a.flags & 2097155)) {
    throw new E.g(2);
  }
  if (1 === (a.flags & 2097155)) {
    throw new E.g(2);
  }
  if (!a.j.la) {
    throw new E.g(43);
  }
  if (!b) {
    throw new E.g(28);
  }
  return a.j.la(a, b, c, d, e);
}, sa(a, b, c, d, e) {
  k(0 <= c);
  return a.j.sa ? a.j.sa(a, b, c, d, e) : 0;
}, Qa(a, b, c) {
  if (!a.j.Qa) {
    throw new E.g(59);
  }
  return a.j.Qa(a, b, c);
}, readFile(a, b = {}) {
  b.flags = b.flags || 0;
  b.encoding = b.encoding || "binary";
  "utf8" !== b.encoding && "binary" !== b.encoding && n(`Invalid encoding type "${b.encoding}"`);
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
  b = Gb(b);
  E.write(a, b, 0, b.byteLength, void 0, c.Fc);
  E.close(a);
}, cwd:() => E.pb, chdir(a) {
  a = H(a, {K:!0});
  if (null === a.node) {
    throw new E.g(44);
  }
  if (!G(a.node.mode)) {
    throw new E.g(54);
  }
  var b = Qb(a.node, "x");
  if (b) {
    throw new E.g(b);
  }
  E.pb = a.path;
}, ed() {
  E.Ba = !1;
  lc(0);
  for (var a of E.streams) {
    a && E.close(a);
  }
}, Pc(a, b) {
  a = fc(a, b);
  return a.exists ? a.object : null;
}, $(a, b, c, d) {
  a = ob("string" == typeof a ? a : Sb(a), b);
  b = Hb(!!c, !!d);
  var e;
  (e = E.$).Ua ?? (e.Ua = 64);
  e = E.ea(E.$.Ua++, 0);
  zb(e, {open(f) {
    f.seekable = !1;
  }, close() {
    d?.buffer?.length && d(10);
  }, read(f, g, m, p) {
    for (var r = 0, t = 0; t < p; t++) {
      try {
        var v = c();
      } catch (y) {
        throw new E.g(29);
      }
      if (void 0 === v && 0 === r) {
        throw new E.g(6);
      }
      if (null === v || void 0 === v) {
        break;
      }
      r++;
      g[m + t] = v;
    }
    r && (f.node.atime = Date.now());
    return r;
  }, write(f, g, m, p) {
    for (var r = 0; r < p; r++) {
      try {
        d(g[m + r]);
      } catch (t) {
        throw new E.g(29);
      }
    }
    p && (f.node.mtime = f.node.ctime = Date.now());
    return r;
  }});
  return cc(a, b, e);
}};
function mc(a, b, c) {
  if ("/" === b.charAt(0)) {
    return b;
  }
  a = -100 === a ? E.cwd() : Xb(a).path;
  if (0 == b.length) {
    if (!c) {
      throw new E.g(44);
    }
    return a;
  }
  return a + "/" + b;
}
function nc(a, b) {
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
var ib = void 0, oc = a => 0 === a % 4 && (0 !== a % 100 || 0 === a % 400), pc = [0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335], qc = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334], J = (a, b, c) => {
  k("number" == typeof c, "stringToUTF8(str, outPtr, maxBytesToWrite) is missing the third parameter that specifies the length of the output buffer!");
  return vb(a, x, b, c);
}, sc = a => {
  a instanceof Ta || "unwind" == a || (wa(), a instanceof WebAssembly.RuntimeError && 0 >= rc() && l("Stack overflow detected.  You can try increasing -sSTACK_SIZE (currently set to 65536)"), ka(1, a));
}, tc = 0, vc = (a, b) => {
  ta = a;
  uc();
  (eb || 0 < tc) && !b && l(`program exited (with status: ${a}), but keepRuntimeAlive() is set (counter=${tc}) due to an async operation, so halting execution but not exiting the runtime or preventing further async execution (you can use emscripten_force_exit, if you want to force a true shutdown)`);
  ta = a;
  eb || 0 < tc || (h.onExit?.(a), sa = !0);
  ka(a, new Ta(a));
}, wc = () => {
  if (!(eb || 0 < tc)) {
    try {
      vc(ta);
    } catch (a) {
      sc(a);
    }
  }
}, xc = a => {
  if (sa) {
    l("user callback triggered after runtime exited or application aborted.  Ignoring.");
  } else {
    try {
      return a();
    } catch (b) {
      sc(b);
    } finally {
      wc();
    }
  }
};
function zc() {
  return document.fullscreenElement || document.mozFullScreenElement || document.webkitFullscreenElement || document.webkitCurrentFullScreenElement || document.msFullscreenElement;
}
var Ac = a => {
  setTimeout(() => {
    xc(a);
  }, 10000);
}, Bc = !1, Cc = [];
function Mb() {
  function a() {
    Bc = document.pointerLockElement === h.canvas;
  }
  if (!Dc) {
    Dc = !0;
    Kb.push({canHandle:c => !h.noImageDecoding && /\.(jpg|jpeg|png|bmp|webp)$/i.test(c), handle:async(c, d) => {
      var e = new Blob([c], {type:Ec(d)});
      e.size !== c.length && (e = new Blob([(new Uint8Array(c)).buffer], {type:Ec(d)}));
      var f = URL.createObjectURL(e);
      return new Promise((g, m) => {
        var p = new Image();
        p.onload = () => {
          k(p.complete, `Image ${d} could not be decoded`);
          var r = document.createElement("canvas");
          r.width = p.width;
          r.height = p.height;
          r.getContext("2d").drawImage(p, 0, 0);
          URL.revokeObjectURL(f);
          g(c);
        };
        p.onerror = () => {
          l(`Image ${f} could not be decoded`);
          m();
        };
        p.src = f;
      });
    }});
    Kb.push({canHandle:c => !h.noAudioDecoding && c.slice(-4) in {".ogg":1, ".wav":1, ".mp3":1}, handle:async(c, d) => new Promise(e => {
      function f() {
        g || (g = !0, e(c));
      }
      var g = !1, m = new Blob([c], {type:Ec(d)});
      m = URL.createObjectURL(m);
      var p = new Audio();
      p.addEventListener("canplaythrough", () => f(p), !1);
      p.onerror = () => {
        if (!g) {
          l(`warning: browser could not fully decode audio ${d}, trying slower base64 approach`);
          for (var r = "data:audio/x-" + d.slice(-3) + ";base64,", t = "", v = 0, y = 0, I = 0; I < c.length; I++) {
            for (v = v << 8 | c[I], y += 8; 6 <= y;) {
              var Q = v >> y - 6 & 63;
              y -= 6;
              t += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[Q];
            }
          }
          2 == y ? (t += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(v & 3) << 4], t += "==") : 4 == y && (t += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(v & 15) << 2], t += "=");
          p.src = r + t;
          f(p);
        }
      };
      p.src = m;
      Ac(() => {
        f(p);
      });
    })});
    var b = h.canvas;
    b && (document.addEventListener("pointerlockchange", a, !1), h.elementPointerLock && b.addEventListener("click", c => {
      !Bc && h.canvas.requestPointerLock && (h.canvas.requestPointerLock(), c.preventDefault());
    }, !1));
  }
}
function Fc(a, b, c, d) {
  if (b && h.ctx && a == h.canvas) {
    return h.ctx;
  }
  var e;
  if (b) {
    var f = {antialias:!1, alpha:!1, Va:"undefined" != typeof WebGL2RenderingContext ? 2 : 1};
    if (d) {
      for (var g in d) {
        f[g] = d[g];
      }
    }
    if ("undefined" != typeof Gc && (e = Hc(a, f))) {
      var m = Ic[e].U;
    }
  } else {
    m = a.getContext("2d");
  }
  if (!m) {
    return null;
  }
  c && (b || k("undefined" == typeof K, "cannot set in module if GLctx is used, but we are a non-GL context that would replace it"), h.ctx = m, b && Jc(e), Cc.forEach(p => p()), Mb());
  return m;
}
function Ec(a) {
  return {jpg:"image/jpeg", jpeg:"image/jpeg", png:"image/png", bmp:"image/bmp", ogg:"audio/ogg", wav:"audio/wav", mp3:"audio/mpeg"}[a.slice(a.lastIndexOf(".") + 1)];
}
var Lb = {}, Dc, L = 12288, Kc = !1, Lc = 0, Mc = 0, Nc = 0, M = {alpha:!1, depth:!1, stencil:!1, antialias:!1}, Oc = {}, Pc, K, Qc = a => {
  var b = a.getExtension("ANGLE_instanced_arrays");
  b && (a.vertexAttribDivisor = (c, d) => b.vertexAttribDivisorANGLE(c, d), a.drawArraysInstanced = (c, d, e, f) => b.drawArraysInstancedANGLE(c, d, e, f), a.drawElementsInstanced = (c, d, e, f, g) => b.drawElementsInstancedANGLE(c, d, e, f, g));
}, Rc = a => {
  var b = a.getExtension("OES_vertex_array_object");
  b && (a.createVertexArray = () => b.createVertexArrayOES(), a.deleteVertexArray = c => b.deleteVertexArrayOES(c), a.bindVertexArray = c => b.bindVertexArrayOES(c), a.isVertexArray = c => b.isVertexArrayOES(c));
}, Sc = a => {
  var b = a.getExtension("WEBGL_draw_buffers");
  b && (a.drawBuffers = (c, d) => b.drawBuffersWEBGL(c, d));
}, Tc = a => {
  var b = "ANGLE_instanced_arrays EXT_blend_minmax EXT_disjoint_timer_query EXT_frag_depth EXT_shader_texture_lod EXT_sRGB OES_element_index_uint OES_fbo_render_mipmap OES_standard_derivatives OES_texture_float OES_texture_half_float OES_texture_half_float_linear OES_vertex_array_object WEBGL_color_buffer_float WEBGL_depth_texture WEBGL_draw_buffers EXT_color_buffer_float EXT_conservative_depth EXT_disjoint_timer_query_webgl2 EXT_texture_norm16 NV_shader_noperspective_interpolation WEBGL_clip_cull_distance EXT_clip_control EXT_color_buffer_half_float EXT_depth_clamp EXT_float_blend EXT_polygon_offset_clamp EXT_texture_compression_bptc EXT_texture_compression_rgtc EXT_texture_filter_anisotropic KHR_parallel_shader_compile OES_texture_float_linear WEBGL_blend_func_extended WEBGL_compressed_texture_astc WEBGL_compressed_texture_etc WEBGL_compressed_texture_etc1 WEBGL_compressed_texture_s3tc WEBGL_compressed_texture_s3tc_srgb WEBGL_debug_renderer_info WEBGL_debug_shaders WEBGL_lose_context WEBGL_multi_draw WEBGL_polygon_mode".split(" ");
  return (a.getSupportedExtensions() || []).filter(c => b.includes(c));
}, Uc = 1, Vc = [], Wc = {}, N = [], Xc = [], Yc = [], Zc = [], O = [], $c = [], Ic = [], P = [], ad = [], bd = [], cd = [], dd = [1, 1, 2, 2, 4, 4, 4, 2, 3, 4, 8], ed = {}, fd = {}, gd = 4, hd = 0, jd = a => {
  for (var b = Uc++, c = a.length; c < b; c++) {
    a[c] = null;
  }
  for (; a[b];) {
    b = Uc++;
  }
  return b;
}, kd = (a, b, c, d) => {
  for (var e = 0; e < a; e++) {
    var f = K[c](), g = f && jd(d);
    f ? (f.name = g, d[g] = f) : T ||= 1282;
    z[b + 4 * e >> 2] = g;
  }
}, ld = a => {
  a = 32 - Math.clz32(0 === a ? 0 : a - 1);
  var b = U.na[a];
  if (b) {
    return b;
  }
  b = K.getParameter(34965);
  U.na[a] = K.createBuffer();
  K.bindBuffer(34963, U.na[a]);
  K.bufferData(34963, 1 << a, 35048);
  K.bindBuffer(34963, b);
  return U.na[a];
}, nd = a => {
  md = !1;
  for (var b = 0; b < U.Wa; ++b) {
    var c = U.V[b];
    if (c.ha && c.enabled) {
      k(a || !K.ja, "must use array buffers when using element buffer");
      md = !0;
      var d = c.size;
      var e = c.type, f = c.stride;
      d = 0 < f ? a * f : d * dd[e - 5120] * a;
      e = 32 - Math.clz32(0 === d ? 0 : d - 1);
      f = U.oa[e];
      var g = U.da[e];
      U.da[e] = U.da[e] + 1 & 63;
      var m = f[g];
      m ? e = m : (m = K.getParameter(34964), f[g] = K.createBuffer(), K.bindBuffer(34962, f[g]), K.bufferData(34962, 1 << e, 35048), K.bindBuffer(34962, m), e = f[g]);
      K.bindBuffer(34962, e);
      K.bufferSubData(34962, 0, x.subarray(c.ma, c.ma + d));
      c.ib.call(K, b, c.size, c.type, c.Za, c.stride, 0);
    }
  }
}, Hc = (a, b) => {
  a.G || (a.G = a.getContext, a.getContext = function(d, e) {
    e = a.G(d, e);
    return "webgl" == d == e instanceof WebGLRenderingContext ? e : null;
  });
  var c = 1 < b.Va ? a.getContext("webgl2", b) : a.getContext("webgl", b);
  return c ? od(c, b) : 0;
}, od = (a, b) => {
  var c = jd(Ic), d = {handle:c, attributes:b, version:b.Va, U:a};
  a.canvas && (a.canvas.pa = d);
  Ic[c] = d;
  if ("undefined" == typeof b.Tb || b.Tb) {
    if ((a = d) || (a = U), !a.cc) {
      a.cc = !0;
      b = a.U;
      b.$c = b.getExtension("WEBGL_multi_draw");
      b.Wb = b.getExtension("EXT_polygon_offset_clamp");
      b.Vb = b.getExtension("EXT_clip_control");
      b.uc = b.getExtension("WEBGL_polygon_mode");
      Qc(b);
      Rc(b);
      Sc(b);
      b.Lc = b.getExtension("WEBGL_draw_instanced_base_vertex_base_instance");
      b.Wc = b.getExtension("WEBGL_multi_draw_instanced_base_vertex_base_instance");
      2 <= a.version && (b.J = b.getExtension("EXT_disjoint_timer_query_webgl2"));
      if (2 > a.version || !b.J) {
        b.J = b.getExtension("EXT_disjoint_timer_query");
      }
      for (var e of Tc(b)) {
        e.includes("lose_context") || e.includes("debug") || b.getExtension(e);
      }
    }
  }
  d.Wa = d.U.getParameter(34921);
  d.V = [];
  for (e = 0; e < d.Wa; e++) {
    d.V[e] = {enabled:!1, ha:!1, size:0, type:0, Za:0, stride:0, ma:0, ib:null};
  }
  d.da = [];
  d.Ga = [];
  d.da.length = d.Ga.length = 22;
  d.oa = [];
  d.ua = [];
  d.oa.length = d.ua.length = 22;
  d.na = [];
  d.na.length = 22;
  for (e = 0; 21 >= e; ++e) {
    d.na[e] = null;
    d.da[e] = d.Ga[e] = 0;
    d.oa[e] = [];
    d.ua[e] = [];
    a = d.oa[e];
    b = d.ua[e];
    a.length = b.length = 64;
    for (var f = 0; 64 > f; ++f) {
      a[f] = b[f] = null;
    }
  }
  return c;
}, Jc = a => {
  U = Ic[a];
  h.ctx = K = U?.U;
}, Gc = {}, T, U, md, pd = a => {
  var b = ub(a) + 1, c = W(b);
  c && J(a, c, b);
  return c;
}, Gd = (a, b, c, d, e) => {
  function f() {
    return g < qd ? (wc(), !1) : !0;
  }
  k(!rd, "emscripten_set_main_loop: there can only be one main loop function at once: call emscripten_cancel_main_loop to cancel the previous one before setting a new one with different parameters.");
  rd = a;
  sd = d;
  var g = qd;
  td = !1;
  ud = function() {
    if (!sa) {
      if (0 < vd.length) {
        var m = vd.shift();
        m.Uc(m.vc);
        if (wd) {
          var p = wd, r = 0 == p % 1 ? p - 1 : Math.floor(p);
          wd = m.Hc ? r : (8 * p + (r + 0.5)) / 9;
        }
        h.setStatus && (m = h.statusMessage || "Please wait...", p = wd ?? 0, r = xd.Nc ?? 0, p ? p < r ? h.setStatus("{message} ({expected - remaining}/{expected})") : h.setStatus(m) : h.setStatus(""));
        f() && setTimeout(ud, 0);
      } else {
        if (f()) {
          if (yd = yd + 1 | 0, 1 == zd && 1 < Ad && 0 != yd % Ad) {
            Bd();
          } else {
            0 == zd && (Cd = performance.now(), h.ctx && Fa("Looks like you are rendering without using requestAnimationFrame for the main loop. You should use 0 for the frame rate in emscripten_set_main_loop in order to use requestAnimationFrame, as that can greatly improve your frame rates!"));
            a: {
              if (!sa) {
                for (m of Dd) {
                  if (!1 === m()) {
                    break a;
                  }
                }
                xc(a);
                for (p of Ed) {
                  p();
                }
                wa();
              }
            }
            f() && Bd();
          }
        }
      }
    }
  };
  e || (0 < b ? Fd(0, 1000.0 / b) : Fd(1, 1), Bd());
  if (c) {
    throw "unwind";
  }
}, td = !1, Bd = null, qd = 0, rd = null, sd = 0, zd = 0, Ad = 0, yd = 0, vd = [], Dd = [], Ed = [];
function Hd() {
  Bd = null;
  qd++;
}
function Id() {
  qd++;
  var a = zd, b = Ad, c = rd;
  rd = null;
  Gd(c, 0, !1, sd, !0);
  Fd(a, b);
  Bd();
}
var Jd = 0;
function Kd(a) {
  if (globalThis.requestAnimationFrame) {
    requestAnimationFrame(a);
  } else {
    var b = Date.now();
    if (0 === Jd) {
      Jd = b + 1000 / 60;
    } else {
      for (; b + 2 >= Jd;) {
        Jd += 1000 / 60;
      }
    }
    setTimeout(a, Math.max(Jd - b, 0));
  }
}
var xd = {}, ud, wd, Cd, Ld, Fd = (a, b) => {
  zd = a;
  Ad = b;
  if (rd) {
    if (td ||= !0, 0 == a) {
      Bd = function() {
        setTimeout(ud, Math.max(0, Cd + b - performance.now()) | 0);
      };
    } else if (1 == a) {
      Bd = function() {
        Kd(ud);
      };
    } else {
      k(2 == a);
      if (!Ld) {
        if (globalThis.setImmediate) {
          Ld = setImmediate;
        } else {
          var c = [];
          addEventListener("message", d => {
            if ("setimmediate" === d.data || "setimmediate" === d.data.target) {
              d.stopPropagation(), c.shift()();
            }
          }, !0);
          Ld = d => {
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
      Bd = function() {
        Ld(ud);
      };
    }
  } else {
    l("emscripten_set_main_loop_timing: Cannot set timing mode for main loop since a main loop does not exist! Call emscripten_set_main_loop first to set one up.");
  }
}, Md = [], Nd = (a, b) => {
  k(Array.isArray(Md));
  k(0 == b % 16);
  Md.length = 0;
  for (var c; c = x[a++];) {
    var d = String.fromCharCode(c), e = ["d", "f", "i", "p"];
    e.push("j");
    k(e.includes(d), `Invalid character ${c}("${d}") in readEmAsmArgs! Use only [${e}], and do not specify "v" for void return argument.`);
    d = 105 != c;
    d &= 112 != c;
    b += d && b % 8 ? 4 : 0;
    Md.push(112 == c ? q[b >> 2] : 106 == c ? Na[b >> 3] : 105 == c ? z[b >> 2] : Ma[b >> 3]);
    b += d ? 8 : 4;
  }
  return Md;
}, Pd = (a, b, c) => {
  b = Nd(b, c);
  k(Od.hasOwnProperty(a), `No EM_ASM constant found at address ${a}.  The loaded WebAssembly file is likely out of sync with the generated JavaScript.`);
  return Od[a](...b);
};
function Qd(a) {
  var b = X.C[a];
  b.target.removeEventListener(b.u, b.sb, b.H);
  X.C.splice(a, 1);
}
function Rd(a, b, c) {
  function d(f, g) {
    if (f.length != g.length) {
      return !1;
    }
    for (var m in f) {
      if (f[m] != g[m]) {
        return !1;
      }
    }
    return !0;
  }
  for (var e of X.aa) {
    if (e.eb == a && d(e.mb, c)) {
      return;
    }
  }
  X.aa.push({eb:a, Ab:b, mb:c});
  X.aa.sort((f, g) => f.Ab - g.Ab);
}
function Sd(a) {
  X.aa = X.aa.filter(b => b.eb != a);
}
function Td() {
  return navigator.userActivation ? navigator.userActivation.isActive : X.Pa && X.Mb.wa;
}
function Ud() {
  if (Td()) {
    var a = X.aa;
    X.aa = [];
    for (var b of a) {
      b.eb(...b.mb);
    }
  }
}
function Vd(a) {
  if (!a.target) {
    return l("registerOrRemoveHandler: the target element for event handler registration does not exist, when processing the following event handler registration:"), console.dir(a), -4;
  }
  if (a.B) {
    a.sb = function(c) {
      ++X.Pa;
      X.Mb = a;
      Ud();
      a.M(c);
      Ud();
      --X.Pa;
    }, a.target.addEventListener(a.u, a.sb, a.H), X.C.push(a);
  } else {
    for (var b = 0; b < X.C.length; ++b) {
      X.C[b].target == a.target && X.C[b].u == a.u && Qd(b--);
    }
  }
  return 0;
}
function Wd(a) {
  return a ? a == window ? "#window" : a == screen ? "#screen" : a?.nodeName || "" : "";
}
var X = {wc:0, Na:0, Sa:0, Fa:0, kb:0, gb:0, La:0, Kc:0, bd:0, Jc:0, Ma:0, $a:0, jb:0, fb:0, gd() {
  for (; X.C.length;) {
    Qd(X.C.length - 1);
  }
  X.aa = [];
}, Pa:0, aa:[], C:[], qc:(a, b) => {
  for (var c = 0; c < X.C.length; ++c) {
    X.C[c].target != a || b && b != X.C[c].u || Qd(c--);
  }
}, hd(a) {
  let b = !1;
  for (let c = 0; c < X.C.length; ++c) {
    const d = X.C[c];
    d.target === a.target && d.F === a.F && d.B === a.B && d.I === a.I && (Qd(c--), b = !0);
  }
  return b ? 0 : -5;
}, fullscreenEnabled() {
  return document.fullscreenEnabled || document.webkitFullscreenEnabled;
}}, Xd = [0, globalThis.document ?? 0, globalThis.window ?? 0], Y = a => {
  a = 2 < a ? D(a) : a;
  return Xd[a] || globalThis.document?.querySelector(a);
}, Zd = a => {
  var b = ub(a) + 1, c = Yd(b);
  J(a, c, b);
  return c;
}, ae = a => {
  var b = rc(), c = Yd(8), d = c + 4;
  a = Zd(a.id);
  if (a = Y(a)) {
    z[c >> 2] = a.width, z[d >> 2] = a.height;
  }
  c = [z[c >> 2], z[d >> 2]];
  $d(b);
  return c;
}, be = (a, b, c) => {
  a = Y(a);
  if (!a) {
    return -4;
  }
  a.width = b;
  a.height = c;
  return 0;
}, ce = (a, b, c) => {
  if (a.Gc) {
    var d = rc();
    a = Zd(a.id);
    be(a, b, c);
    $d(d);
  } else {
    a.width = b, a.height = c;
  }
}, de = {}, fe = a => {
  function b() {
    zc() || (document.removeEventListener("fullscreenchange", b), document.removeEventListener("webkitfullscreenchange", b), ce(a, d, e), a.style.width = f, a.style.height = g, a.style.backgroundColor = m, p || (document.body.style.backgroundColor = "white"), document.body.style.backgroundColor = p, a.style.paddingLeft = r, a.style.paddingRight = t, a.style.paddingTop = v, a.style.paddingBottom = y, a.style.marginLeft = I, a.style.marginRight = Q, a.style.marginTop = R, a.style.marginBottom = aa, 
    document.body.style.margin = S, document.documentElement.style.overflow = da, document.body.scroll = yc, a.style.ba = V, a.pa && a.pa.U.viewport(0, 0, d, e), de.xa && ee(de.xa, 37, 0, de.nb));
  }
  var c = ae(a), d = c[0], e = c[1], f = a.style.width, g = a.style.height, m = a.style.backgroundColor, p = document.body.style.backgroundColor, r = a.style.paddingLeft, t = a.style.paddingRight, v = a.style.paddingTop, y = a.style.paddingBottom, I = a.style.marginLeft, Q = a.style.marginRight, R = a.style.marginTop, aa = a.style.marginBottom, S = document.body.style.margin, da = document.documentElement.style.overflow, yc = document.body.scroll, V = a.style.ba;
  document.addEventListener("fullscreenchange", b);
  document.addEventListener("webkitfullscreenchange", b);
}, ge = (a, b, c) => {
  a.style.paddingLeft = a.style.paddingRight = c + "px";
  a.style.paddingTop = a.style.paddingBottom = b + "px";
}, he = a => 0 > Xd.indexOf(a) ? a.getBoundingClientRect() : {left:0, top:0}, ie = (a, b) => {
  if (0 != b.bb || 0 != b.Ha) {
    fe(a);
    var c = b.rc ? innerWidth : screen.width, d = b.rc ? innerHeight : screen.height, e = he(a), f = e.width;
    e = e.height;
    var g = ae(a), m = g[0];
    g = g[1];
    3 == b.bb ? (ge(a, (d - e) / 2, (c - f) / 2), c = f, d = e) : 2 == b.bb && (c * g < m * d ? (f = g * c / m, ge(a, (d - f) / 2, 0), d = f) : (f = m * d / g, ge(a, 0, (c - f) / 2), c = f));
    var p;
    (p = a.style).backgroundColor || (p.backgroundColor = "black");
    let r;
    (r = document.body.style).backgroundColor || (r.backgroundColor = "black");
    a.style.width = c + "px";
    a.style.height = d + "px";
    1 == b.Yb && (a.style.ba = "optimizeSpeed", a.style.ba = "-moz-crisp-edges", a.style.ba = "-o-crisp-edges", a.style.ba = "-webkit-optimize-contrast", a.style.ba = "optimize-contrast", a.style.ba = "crisp-edges", a.style.ba = "pixelated");
    p = 2 == b.Ha ? devicePixelRatio : 1;
    0 != b.Ha && (c = c * p | 0, d = d * p | 0, ce(a, c, d), a.pa && a.pa.U.viewport(0, 0, c, d));
  }
  if (a.requestFullscreen) {
    a.requestFullscreen();
  } else if (a.webkitRequestFullscreen) {
    a.webkitRequestFullscreen(Element.ALLOW_KEYBOARD_INPUT);
  } else {
    return X.fullscreenEnabled() ? -3 : -1;
  }
  de = b;
  b.xa && ee(b.xa, 37, 0, b.nb);
  return 0;
}, je = a => {
  if (a.requestPointerLock) {
    a.requestPointerLock();
  } else {
    return document.body.requestPointerLock ? -3 : -1;
  }
  return 0;
}, ke = (a, b) => {
  Ma[a >> 3] = b.timestamp;
  for (var c = 0; c < b.axes.length; ++c) {
    Ma[a + 8 * c + 16 >> 3] = b.axes[c];
  }
  for (c = 0; c < b.buttons.length; ++c) {
    Ma[a + 8 * c + 528 >> 3] = "object" == typeof b.buttons[c] ? b.buttons[c].value : b.buttons[c];
  }
  for (c = 0; c < b.buttons.length; ++c) {
    w[a + c + 1040] = "object" == typeof b.buttons[c] ? b.buttons[c].pressed : 1 == b.buttons[c];
  }
  w[a + 1104] = b.connected;
  z[a + 1108 >> 2] = b.index;
  z[a + 8 >> 2] = b.axes.length;
  z[a + 12 >> 2] = b.buttons.length;
  J(b.id, a + 1112, 64);
  J(b.mapping, a + 1176, 64);
}, le = a => K.activeTexture(a), me = (a, b) => {
  K.attachShader(N[a], O[b]);
}, ne = (a, b, c) => {
  K.bindAttribLocation(N[a], b, D(c));
}, oe = (a, b) => {
  if (b && !Vc[b]) {
    var c = K.createBuffer();
    c.name = b;
    Vc[b] = c;
  }
  34962 == a ? K.ia = b : 34963 == a && (K.ja = b);
  35051 == a ? K.Ka = b : 35052 == a && (K.S = b);
  K.bindBuffer(a, Vc[b]);
}, pe = (a, b) => {
  K.bindFramebuffer(a, Xc[b]);
}, qe = (a, b) => {
  K.bindRenderbuffer(a, Yc[b]);
}, re = (a, b) => {
  K.bindTexture(a, Zc[b]);
}, se = a => {
  K.bindVertexArray($c[a]);
  a = K.getParameter(34965);
  K.ja = a ? a.name | 0 : 0;
}, te = (a, b, c, d) => K.blendFuncSeparate(a, b, c, d), ue = (a, b, c, d) => {
  2 <= U.version ? c && b ? K.bufferData(a, x, d, c, b) : K.bufferData(a, b, d) : K.bufferData(a, c ? x.subarray(c, c + b) : b, d);
}, ve = a => K.checkFramebufferStatus(a), we = a => K.clear(a), xe = (a, b, c, d) => K.clearColor(a, b, c, d), ye = (a, b, c, d) => {
  K.colorMask(!!a, !!b, !!c, !!d);
}, ze = a => {
  K.compileShader(O[a]);
}, Ae = () => {
  var a = jd(N), b = K.createProgram();
  b.name = a;
  b.Ea = b.Ca = b.Da = 0;
  b.hb = 1;
  N[a] = b;
  return a;
}, Be = a => {
  var b = jd(O);
  O[b] = K.createShader(a);
  return b;
}, Ce = a => K.cullFace(a), De = (a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = Vc[d];
    e && (K.deleteBuffer(e), e.name = 0, Vc[d] = null, d == K.ia && (K.ia = 0), d == K.ja && (K.ja = 0), d == K.Ka && (K.Ka = 0), d == K.S && (K.S = 0));
  }
}, Ee = (a, b) => {
  for (var c = 0; c < a; ++c) {
    var d = z[b + 4 * c >> 2], e = Xc[d];
    e && (K.deleteFramebuffer(e), e.name = 0, Xc[d] = null);
  }
}, Fe = a => {
  if (a) {
    var b = N[a];
    b ? (K.deleteProgram(b), b.name = 0, N[a] = null) : T ||= 1281;
  }
}, Ge = (a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = Yc[d];
    e && (K.deleteRenderbuffer(e), e.name = 0, Yc[d] = null);
  }
}, He = a => {
  if (a) {
    var b = O[a];
    b ? (K.deleteShader(b), O[a] = null) : T ||= 1281;
  }
}, Ie = (a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = Zc[d];
    e && (K.deleteTexture(e), e.name = 0, Zc[d] = null);
  }
}, Je = (a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2];
    K.deleteVertexArray($c[d]);
    $c[d] = null;
  }
}, Ke = a => K.disable(a), Le = a => {
  U.V[a].enabled = !1;
  K.disableVertexAttribArray(a);
}, Me = (a, b, c) => {
  nd(b + c);
  K.drawArrays(a, b, c);
  md && K.bindBuffer(34962, Vc[K.ia]);
}, Ne = (a, b, c, d) => {
  K.drawArraysInstanced(a, b, c, d);
}, Oe = [], Pe = (a, b) => {
  for (var c = Oe[a], d = 0; d < a; d++) {
    c[d] = z[b + 4 * d >> 2];
  }
  K.drawBuffers(c);
}, Qe = (a, b, c, d) => {
  var e = 0;
  if (!K.ja) {
    var f = 1 * dd[c - 5120] * b;
    var g = ld(f);
    K.bindBuffer(34963, g);
    K.bufferSubData(34963, 0, x.subarray(d, d + f));
    if (0 < b) {
      for (g = 0; g < U.Wa; ++g) {
        if (f = U.V[g], f.ha && f.enabled) {
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
          e = (new e(x.buffer, d, b)).reduce((m, p) => Math.max(m, p)) + 1;
          break;
        }
      }
    }
    d = 0;
  }
  nd(e);
  K.drawElements(a, b, c, d);
  md && K.bindBuffer(34962, Vc[K.ia]);
  K.ja || K.bindBuffer(34963, null);
}, Re = (a, b, c, d, e) => {
  K.drawElementsInstanced(a, b, c, d, e);
}, Se = a => K.enable(a), Te = a => {
  U.V[a].enabled = !0;
  K.enableVertexAttribArray(a);
}, Ue = () => K.finish(), Ve = a => {
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
  return (a = K.getParameter(a)) ? a.name | 0 : 0;
}, We = a => {
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
}, Xe = (a, b, c, d) => {
  K.framebufferRenderbuffer(a, b, c, Yc[d]);
}, Ye = (a, b, c, d, e) => {
  K.framebufferTexture2D(a, b, c, Zc[d], e);
}, Ze = a => K.frontFace(a), $e = (a, b) => {
  kd(a, b, "createBuffer", Vc);
}, af = (a, b) => {
  kd(a, b, "createFramebuffer", Xc);
}, bf = (a, b) => {
  kd(a, b, "createRenderbuffer", Yc);
}, cf = (a, b) => {
  kd(a, b, "createTexture", Zc);
}, df = (a, b) => {
  kd(a, b, "createVertexArray", $c);
}, ef = a => K.generateMipmap(a), ff = (a, b, c, d, e, f, g, m) => {
  b = N[b];
  if (a = K[a](b, c)) {
    d = m && J(a.name, m, d), e && (z[e >> 2] = d), f && (z[f >> 2] = a.size), g && (z[g >> 2] = a.type);
  }
}, gf = (a, b) => {
  q[a >> 2] = b;
  q[a + 4 >> 2] = (b - q[a >> 2]) / 4294967296;
  var c = 0 <= b ? q[a >> 2] + 4294967296 * q[a + 4 >> 2] : q[a >> 2] + 4294967296 * z[a + 4 >> 2];
  a >>= 2;
  c != b && Fa(`writeI53ToI64() out of range: serialized JS Number ${b} to Wasm heap as bytes lo=${xa(q[a])}, hi=${xa(q[a + 1])}, which deserializes back to ${c} instead!`);
}, hf = () => {
  var a = Tc(K);
  return a = a.concat(a.map(b => "GL_" + b));
}, jf = (a, b, c) => {
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
        var e = K.getParameter(34467);
        d = e ? e.length : 0;
        break;
      case 33309:
        if (2 > U.version) {
          T ||= 1282;
          return;
        }
        d = hf().length;
        break;
      case 33307:
      case 33308:
        if (2 > U.version) {
          T ||= 1280;
          return;
        }
        d = 33307 == a ? 3 : 0;
    }
    if (void 0 === d) {
      switch(e = K.getParameter(a), typeof e) {
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
                    w[b + a] = e[a] ? 1 : 0;
                }
              }
              return;
            }
            try {
              d = e.name | 0;
            } catch (f) {
              T ||= 1280;
              l(`GL_INVALID_ENUM in glGet${c}v: Unknown object returned from WebGL getParameter(${a})! (error: ${f})`);
              return;
            }
          }
          break;
        default:
          T ||= 1280;
          l(`GL_INVALID_ENUM in glGet${c}v: Native code calling glGet${c}v(${a}) and it returns ${e} of type ${typeof e}!`);
          return;
      }
    }
    switch(c) {
      case 1:
        gf(b, d);
        break;
      case 0:
        z[b >> 2] = d;
        break;
      case 2:
        A[b >> 2] = d;
        break;
      case 4:
        w[b] = d ? 1 : 0;
    }
  } else {
    T ||= 1281;
  }
}, kf = () => {
  var a = K.getError() || T;
  T = 0;
  return a;
}, lf = (a, b, c, d) => {
  if (c) {
    b = K.getIndexedParameter(a, b);
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
        gf(c, a);
        break;
      case 0:
        z[c >> 2] = a;
        break;
      case 2:
        A[c >> 2] = a;
        break;
      case 4:
        w[c] = a ? 1 : 0;
        break;
      default:
        n("internal emscriptenWebGLGetIndexed() error, bad type: " + d);
    }
  } else {
    T ||= 1281;
  }
}, mf = (a, b) => jf(a, b, 0), nf = (a, b, c, d) => {
  a = K.getProgramInfoLog(N[a]);
  null === a && (a = "(unknown error)");
  b = 0 < b && d ? J(a, d, b) : 0;
  c && (z[c >> 2] = b);
}, of = (a, b, c) => {
  if (c) {
    if (a >= Uc) {
      T ||= 1281;
    } else {
      if (a = N[a], 35716 == b) {
        a = K.getProgramInfoLog(a), null === a && (a = "(unknown error)"), z[c >> 2] = a.length + 1;
      } else if (35719 == b) {
        if (!a.Ea) {
          var d = K.getProgramParameter(a, 35718);
          for (b = 0; b < d; ++b) {
            a.Ea = Math.max(a.Ea, K.getActiveUniform(a, b).name.length + 1);
          }
        }
        z[c >> 2] = a.Ea;
      } else if (35722 == b) {
        if (!a.Ca) {
          for (d = K.getProgramParameter(a, 35721), b = 0; b < d; ++b) {
            a.Ca = Math.max(a.Ca, K.getActiveAttrib(a, b).name.length + 1);
          }
        }
        z[c >> 2] = a.Ca;
      } else if (35381 == b) {
        if (!a.Da) {
          for (d = K.getProgramParameter(a, 35382), b = 0; b < d; ++b) {
            a.Da = Math.max(a.Da, K.getActiveUniformBlockName(a, b).length + 1);
          }
        }
        z[c >> 2] = a.Da;
      } else {
        z[c >> 2] = K.getProgramParameter(a, b);
      }
    }
  } else {
    T ||= 1281;
  }
}, pf = (a, b, c) => {
  if (c) {
    a = P[a];
    b = 2 > U.version ? K.J.getQueryObjectEXT(a, b) : K.getQueryParameter(a, b);
    var d;
    "boolean" == typeof b ? d = b ? 1 : 0 : d = b;
    gf(c, d);
  } else {
    T ||= 1281;
  }
}, qf = (a, b, c) => {
  if (c) {
    a = K.J.getQueryObjectEXT(P[a], b);
    var d;
    "boolean" == typeof a ? d = a ? 1 : 0 : d = a;
    z[c >> 2] = d;
  } else {
    T ||= 1281;
  }
}, rf = (a, b, c, d) => {
  a = K.getShaderInfoLog(O[a]);
  null === a && (a = "(unknown error)");
  b = 0 < b && d ? J(a, d, b) : 0;
  c && (z[c >> 2] = b);
}, sf = (a, b, c) => {
  c ? 35716 == b ? (a = K.getShaderInfoLog(O[a]), null === a && (a = "(unknown error)"), z[c >> 2] = a ? a.length + 1 : 0) : 35720 == b ? (a = K.getShaderSource(O[a]), z[c >> 2] = a ? a.length + 1 : 0) : z[c >> 2] = K.getShaderParameter(O[a], b) : T ||= 1281;
}, tf = a => "]" == a.slice(-1) && a.lastIndexOf("["), uf = a => {
  var b = a.va, c = a.Fb, d;
  if (!b) {
    a.va = b = {};
    a.Eb = {};
    var e = K.getProgramParameter(a, 35718);
    for (d = 0; d < e; ++d) {
      var f = K.getActiveUniform(a, d);
      var g = f.name;
      f = f.size;
      var m = tf(g);
      m = 0 < m ? g.slice(0, m) : g;
      var p = a.hb;
      a.hb += f;
      c[m] = [f, p];
      for (g = 0; g < f; ++g) {
        b[p] = g, a.Eb[p++] = m;
      }
    }
  }
}, vf = (a, b) => {
  b = D(b);
  if (a = N[a]) {
    uf(a);
    var c = a.va, d = 0, e = b, f = tf(b);
    0 < f && (d = parseInt(b.slice(f + 1)) >>> 0, e = b.slice(0, f));
    if ((e = a.Fb[e]) && d < e[0] && (d += e[1], c[d] = c[d] || K.getUniformLocation(a, b))) {
      return d;
    }
  } else {
    T ||= 1281;
  }
  return -1;
}, Z = a => {
  var b = K.Nb;
  if (b) {
    var c = b.va[a];
    "number" == typeof c && (b.va[a] = c = K.getUniformLocation(b, b.Eb[a] + (0 < c ? `[${c}]` : "")));
    return c;
  }
  T ||= 1282;
}, wf = (a, b, c, d) => {
  if (c) {
    if (a = N[a], uf(a), a = K.getUniform(a, Z(b)), "number" == typeof a || "boolean" == typeof a) {
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
}, xf = (a, b, c, d) => {
  if (c) {
    if (U.V[a].enabled && l("glGetVertexAttrib*v on client-side array: not supported, bad data returned"), a = K.getVertexAttrib(a, b), 34975 == b) {
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
}, yf = (a, b, c) => {
  xf(a, b, c, 0);
}, zf = a => (a = $c[a]) ? K.isVertexArray(a) : 0, Af = a => {
  a = N[a];
  K.linkProgram(a);
  a.va = 0;
  a.Fb = {};
}, Bf = (a, b) => {
  3317 == a ? gd = b : 3314 == a && (hd = b);
  K.pixelStorei(a, b);
}, Cf = a => {
  a -= 5120;
  return 0 == a ? w : 1 == a ? x : 2 == a ? Ka : 4 == a ? z : 6 == a ? A : 5 == a || 28922 == a || 28520 == a || 30779 == a || 30782 == a ? q : La;
}, Df = (a, b, c, d, e) => {
  a = Cf(a);
  b = d * ((hd || c) * ({5:3, 6:4, 8:2, 29502:3, 29504:4, 26917:2, 26918:2, 29846:3, 29847:4}[b - 6402] || 1) * a.BYTES_PER_ELEMENT + gd - 1 & -gd);
  return a.subarray(e >>> 31 - Math.clz32(a.BYTES_PER_ELEMENT), e + b >>> 31 - Math.clz32(a.BYTES_PER_ELEMENT));
}, Ef = (a, b, c, d, e, f, g) => {
  if (2 <= U.version) {
    if (K.Ka) {
      K.readPixels(a, b, c, d, e, f, g);
    } else {
      var m = Cf(f);
      g >>>= 31 - Math.clz32(m.BYTES_PER_ELEMENT);
      K.readPixels(a, b, c, d, e, f, m, g);
    }
  } else {
    (m = Df(f, e, c, d, g)) ? K.readPixels(a, b, c, d, e, f, m) : T ||= 1280;
  }
}, Ff = (a, b, c, d) => K.renderbufferStorage(a, b, c, d), Gf = (a, b, c, d) => {
  for (var e = "", f = 0; f < b; ++f) {
    e += D(q[c + 4 * f >> 2], d ? q[d + 4 * f >> 2] : void 0);
  }
  K.shaderSource(O[a], e);
}, Hf = (a, b, c) => K.stencilFunc(a, b, c), If = a => K.stencilMask(a), Jf = (a, b, c) => K.stencilOp(a, b, c), Kf = (a, b, c, d) => K.stencilOpSeparate(a, b, c, d), Lf = (a, b, c, d, e, f, g, m, p) => {
  if (2 <= U.version) {
    if (K.S) {
      K.texImage2D(a, b, c, d, e, f, g, m, p);
      return;
    }
    if (p) {
      var r = Cf(m);
      p >>>= 31 - Math.clz32(r.BYTES_PER_ELEMENT);
      K.texImage2D(a, b, c, d, e, f, g, m, r, p);
      return;
    }
  }
  r = p ? Df(m, g, d, e, p) : null;
  K.texImage2D(a, b, c, d, e, f, g, m, r);
}, Mf = (a, b, c) => K.texParameteri(a, b, c), Nf = (a, b, c, d, e, f, g, m, p) => {
  if (2 <= U.version) {
    if (K.S) {
      K.texSubImage2D(a, b, c, d, e, f, g, m, p);
      return;
    }
    if (p) {
      var r = Cf(m);
      K.texSubImage2D(a, b, c, d, e, f, g, m, r, p >>> 31 - Math.clz32(r.BYTES_PER_ELEMENT));
      return;
    }
  }
  p = p ? Df(m, g, e, f, p) : null;
  K.texSubImage2D(a, b, c, d, e, f, g, m, p);
}, Of = [], Pf = (a, b) => {
  K.uniform1i(Z(a), b);
}, Qf = [], Rf = (a, b, c) => {
  if (2 <= U.version) {
    b && K.uniform2fv(Z(a), A, c >> 2, 2 * b);
  } else {
    if (144 >= b) {
      b *= 2;
      for (var d = Of[b], e = 0; e < b; e += 2) {
        d[e] = A[c + 4 * e >> 2], d[e + 1] = A[c + (4 * e + 4) >> 2];
      }
    } else {
      d = A.subarray(c >> 2, c + 8 * b >> 2);
    }
    K.uniform2fv(Z(a), d);
  }
}, Sf = (a, b, c) => {
  if (2 <= U.version) {
    b && K.uniform4fv(Z(a), A, c >> 2, 4 * b);
  } else {
    if (72 >= b) {
      var d = Of[4 * b], e = A;
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
    K.uniform4fv(Z(a), d);
  }
}, Tf = a => {
  a = N[a];
  K.useProgram(a);
  K.Nb = a;
}, Uf = (a, b) => {
  K.vertexAttribDivisor(a, b);
}, Vf = (a, b, c, d, e, f) => {
  var g = U.V[a];
  K.ia ? (g.ha = !1, K.vertexAttribPointer(a, b, c, !!d, e, f)) : (g.size = b, g.type = c, g.Za = d, g.stride = e, g.ma = f, g.ha = !0, g.ib = function(m, p, r, t, v, y) {
    this.vertexAttribPointer(m, p, r, t, v, y);
  });
}, Wf = (a, b, c, d) => K.viewport(a, b, c, d), Xf = (a, b) => {
  if (!X.fullscreenEnabled()) {
    return -1;
  }
  a = Y(a);
  return a ? a.requestFullscreen || a.webkitRequestFullscreen ? Td() ? ie(a, b) : b.Rb ? (Rd(ie, 1, [a, b]), 1) : -2 : -3 : -4;
}, Yf = () => {
  try {
    if (navigator.getGamepads) {
      return (X.ka = navigator.getGamepads()) ? 0 : -1;
    }
  } catch (a) {
    l(`navigator.getGamepads() exists, but failed to execute with exception ${a}. Disabling Gamepad access.`), navigator.getGamepads = null;
  }
  return -1;
}, Zf = (a, b) => {
  var c = {target:Y(2), u:"beforeunload", F:28, I:a, B:b, M:d => {
    var e = ee(b, 28, 0, a);
    e &&= D(e);
    if (e) {
      return d.preventDefault(), d.returnValue = e;
    }
  }, H:!0};
  return Vd(c);
}, $f = (a, b, c, d, e, f) => {
  X.La || (X.La = W(256));
  a = {target:Y(a), u:f, F:e, I:b, B:d, M:g => {
    var m = g.target.id ? g.target.id : "", p = X.La;
    J(Wd(g.target), p + 0, 128);
    J(m, p + 128, 128);
    ee(d, e, p, b) && g.preventDefault();
  }, H:c};
  return Vd(a);
}, ag = (a, b, c, d, e) => {
  X.Ma || (X.Ma = W(276));
  return Vd({target:a, u:e, F:19, I:b, B:d, M:f => {
    var g = X.Ma, m = zc(), p = !!m;
    w[g] = p;
    w[g + 1] = X.fullscreenEnabled();
    var r = p ? m : X.pc, t = r?.id || "";
    J(Wd(r), g + 2, 128);
    J(t, g + 130, 128);
    z[g + 260 >> 2] = r ? r.clientWidth : 0;
    z[g + 264 >> 2] = r ? r.clientHeight : 0;
    z[g + 268 >> 2] = screen.width;
    z[g + 272 >> 2] = screen.height;
    p && (X.pc = m);
    ee(d, 19, g, b) && f.preventDefault();
  }, H:c});
}, bg = (a, b, c, d, e) => {
  X.Na || (X.Na = W(1240));
  b = {target:Y(2), wa:!0, u:e, F:d, I:a, B:c, M:f => {
    var g = X.Na;
    ke(g, f.gamepad);
    ee(c, d, g, a) && f.preventDefault();
  }, H:b};
  return Vd(b);
}, cg = (a, b, c, d, e, f) => {
  X.Sa || (X.Sa = W(160));
  a = {target:Y(a), u:f, F:e, I:b, B:d, M:g => {
    k(g);
    var m = X.Sa;
    Ma[m >> 3] = g.timeStamp;
    var p = m >> 2;
    z[p + 2] = g.location;
    w[m + 12] = g.ctrlKey;
    w[m + 13] = g.shiftKey;
    w[m + 14] = g.altKey;
    w[m + 15] = g.metaKey;
    w[m + 16] = g.repeat;
    z[p + 5] = g.charCode;
    z[p + 6] = g.keyCode;
    z[p + 7] = g.which;
    J(g.key || "", m + 32, 32);
    J(g.code || "", m + 64, 32);
    J(g.char || "", m + 96, 32);
    J(g.locale || "", m + 128, 32);
    ee(d, e, m, b) && g.preventDefault();
  }, H:c};
  return Vd(a);
}, dg = (a, b, c) => {
  k(0 == a % 4);
  Ma[a >> 3] = b.timeStamp;
  var d = a >> 2;
  z[d + 2] = b.screenX;
  z[d + 3] = b.screenY;
  z[d + 4] = b.clientX;
  z[d + 5] = b.clientY;
  w[a + 24] = b.ctrlKey;
  w[a + 25] = b.shiftKey;
  w[a + 26] = b.altKey;
  w[a + 27] = b.metaKey;
  Ka[2 * d + 14] = b.button;
  Ka[2 * d + 15] = b.buttons;
  z[d + 8] = b.movementX;
  z[d + 9] = b.movementY;
  a = he(c);
  z[d + 10] = b.clientX - (a.left | 0);
  z[d + 11] = b.clientY - (a.top | 0);
}, eg = (a, b, c, d, e, f) => {
  X.Fa || (X.Fa = W(64));
  a = Y(a);
  return Vd({target:a, wa:"mousemove" != f && "mouseenter" != f && "mouseleave" != f, u:f, F:e, I:b, B:d, M:g => {
    dg(X.Fa, g, a);
    ee(d, e, X.Fa, b) && g.preventDefault();
  }, H:c});
}, fg = (a, b, c, d) => {
  X.$a || (X.$a = W(257));
  return Vd({target:a, u:"pointerlockchange", F:20, I:b, B:d, M:e => {
    var f = X.$a, g = document.pointerLockElement;
    w[f] = !!g;
    var m = g?.id || "";
    J(Wd(g), f + 1, 128);
    J(m, f + 129, 128);
    ee(d, 20, f, b) && e.preventDefault();
  }, H:c});
}, gg = (a, b, c, d) => {
  X.gb || (X.gb = W(36));
  a = Y(a);
  return Vd({target:a, u:"resize", F:10, I:b, B:d, M:e => {
    if (e.target == a) {
      var f = document.body;
      if (f) {
        var g = X.gb;
        z[g >> 2] = 0;
        z[g + 4 >> 2] = f.clientWidth;
        z[g + 8 >> 2] = f.clientHeight;
        z[g + 12 >> 2] = innerWidth;
        z[g + 16 >> 2] = innerHeight;
        z[g + 20 >> 2] = outerWidth;
        z[g + 24 >> 2] = outerHeight;
        z[g + 28 >> 2] = pageXOffset | 0;
        z[g + 32 >> 2] = pageYOffset | 0;
        ee(d, 10, g, b) && e.preventDefault();
      }
    }
  }, H:c});
}, hg = (a, b, c, d, e, f) => {
  X.fb || (X.fb = W(1552));
  a = Y(a);
  return Vd({target:a, wa:"touchstart" == f || "touchend" == f, u:f, F:e, I:b, B:d, M:g => {
    k(g);
    var m = {}, p = g.touches;
    for (var r of p) {
      r.vb = r.zb = 0, m[r.identifier] = r;
    }
    for (var t of g.changedTouches) {
      t.vb = 1, m[t.identifier] = t;
    }
    for (var v of g.targetTouches) {
      m[v.identifier].zb = 1;
    }
    p = X.fb;
    Ma[p >> 3] = g.timeStamp;
    w[p + 12] = g.ctrlKey;
    w[p + 13] = g.shiftKey;
    w[p + 14] = g.altKey;
    w[p + 15] = g.metaKey;
    r = p + 16;
    t = he(a);
    v = 0;
    for (let y of Object.values(m)) {
      if (m = r >> 2, z[m] = y.identifier, z[m + 1] = y.screenX, z[m + 2] = y.screenY, z[m + 3] = y.clientX, z[m + 4] = y.clientY, z[m + 5] = y.pageX, z[m + 6] = y.pageY, w[r + 28] = y.vb, w[r + 29] = y.zb, z[m + 8] = y.clientX - (t.left | 0), z[m + 9] = y.clientY - (t.top | 0), r += 48, 31 < ++v) {
        break;
      }
    }
    z[p + 8 >> 2] = v;
    ee(d, e, p, b) && g.preventDefault();
  }, H:c});
}, ig = (a, b, c) => {
  var d = Xd[1];
  X.jb || (X.jb = W(8));
  return Vd({target:d, u:"visibilitychange", F:21, I:a, B:c, M:e => {
    var f = X.jb, g = ["hidden", "visible", "prerender", "unloaded"].indexOf(document.visibilityState);
    w[f] = document.hidden;
    z[f + 4 >> 2] = g;
    ee(c, 21, f, a) && e.preventDefault();
  }, H:b});
}, jg = (a, b, c, d) => {
  X.kb || (X.kb = W(96));
  return Vd({target:a, wa:!0, u:"wheel", F:9, I:b, B:d, M:e => {
    var f = X.kb;
    dg(f, e, a);
    Ma[f + 64 >> 3] = e.deltaX;
    Ma[f + 72 >> 3] = e.deltaY;
    Ma[f + 80 >> 3] = e.deltaZ;
    z[f + 88 >> 2] = e.deltaMode;
    ee(d, 9, f, b) && e.preventDefault();
  }, H:c});
};
function kg(a) {
  return lg(() => new Promise(b => setTimeout(b, a)));
}
kg.hc = !0;
var mg = {}, og = () => {
  if (!ng) {
    var a = {USER:"web_user", LOGNAME:"web_user", PATH:"/", PWD:"/", HOME:"/home/web_user", LANG:(globalThis.navigator?.language ?? "C").replace("-", "_") + ".UTF-8", _:ja || "./this.program"}, b;
    for (b in mg) {
      void 0 === mg[b] ? delete a[b] : a[b] = mg[b];
    }
    var c = [];
    for (b in a) {
      c.push(`${b}=${a[b]}`);
    }
    ng = c;
  }
  return ng;
}, ng, pg = a => {
  for (var b of ["keydown", "mousedown", "touchstart"]) {
    for (var c of [document, document.getElementById("canvas")]) {
      c?.addEventListener(b, () => {
        "suspended" === a.state && a.resume();
      }, {once:!0});
    }
  }
}, qg = a => {
  try {
    a();
  } catch (b) {
    n(b);
  }
};
function rg() {
  var a = sg, b = /^(invoke_.*|__asyncjs__.*)$/;
  for (let [c, d] of Object.entries(a)) {
    if ("function" == typeof d) {
      let e = d.hc || b.test(c);
      a[c] = (...f) => {
        var g = tg;
        try {
          return d(...f);
        } finally {
          f = g === ug && tg === vg;
          var m = c.startsWith("invoke_") && !0;
          tg === g || e || f || m || n(`import ${c} was not in ASYNCIFY_IMPORTS, but changed the state`);
        }
      };
    }
  }
}
function wg(a) {
  var b = (...c) => {
    xg.push(a);
    try {
      return a(...c);
    } finally {
      sa || (c = xg.pop(), k(c === a), yg && 1 === tg && 0 === xg.length && (tg = ug, qg(zg), "undefined" != typeof Fibers && Fibers.kd()));
    }
  };
  Ag.set(a, b);
  return b = Object.defineProperty(b, "name", {value:`__asyncify_wrapper_${a.name}`});
}
var ug = 0, vg = 3, tg = 0, yg = null, Bg = 0, xg = [], Cg = new Map(), Dg = new Map(), Ag = new Map(), Eg = 0, Fg = null, Gg = [];
function Hg() {
  k(yg, "Tried to wait for an async operation when none is in progress.");
  k(!Fg, "Cannot have multiple async operations in flight at once");
  return new Promise((a, b) => {
    Fg = {resolve:a, reject:b};
  });
}
function Ig() {
  var a = W(65548), b = a + 12;
  q[a >> 2] = b;
  q[a + 4 >> 2] = b + 65536;
  b = xg[0];
  k(b, "exportCallStack is empty");
  k(b);
  if (!Cg.has(b)) {
    var c = Eg++;
    Cg.set(b, c);
    Dg.set(c, b);
  }
  b = Cg.get(b);
  z[a + 8 >> 2] = b;
  return a;
}
function Jg() {
  var a = z[yg + 8 >> 2], b = Dg.get(a);
  k(b, `id ${a} not found in callStackIdToFunc`);
  a = Ag.get(b);
  k(b);
  k(a);
  return xc(a);
}
function Kg(a) {
  k(tg !== vg, "Asyncify cannot be done during or after the runtime exits");
  if (!sa) {
    if (tg === ug) {
      var b = !1, c = !1;
      a((d = 0) => {
        k(["undefined", "number", "boolean", "bigint"].includes(typeof d), `invalid type for handleSleepReturnValue: '${typeof d}'`);
        if (!sa && (Bg = d, b = !0, c)) {
          k(!xg.length, "Waking up (starting to rewind) must be done from JS, without compiled code on the stack.");
          tg = 2;
          qg(() => Lg(yg));
          "undefined" != typeof xd && rd && Id();
          d = !1;
          try {
            var e = Jg();
          } catch (m) {
            e = m, d = !0;
          }
          var f = !1;
          if (!yg) {
            var g = Fg;
            g && (Fg = null, (d ? g.reject : g.resolve)(e), f = !0);
          }
          if (d && !f) {
            throw e;
          }
        }
      });
      c = !0;
      b || (tg = 1, yg = Ig(), "undefined" != typeof xd && rd && Hd(), qg(() => Mg(yg)));
    } else {
      2 === tg ? (tg = ug, qg(Ng), Og(yg), yg = null, Gg.forEach(xc)) : n(`invalid state: ${tg}`);
    }
    return Bg;
  }
}
var lg = a => Kg(async b => {
  b(await a());
}), Pg = a => {
  var b = h["_" + a];
  k(b, `Cannot call unknown function ${a}, make sure it is exported`);
  return b;
};
E.Ic = (a, b, c, d, e, f, g, m, p, r) => {
  Pb(a, b, c, d, e, m, p, r).then(f).catch(g);
};
E.cd = Pb;
E.Y = Array(4096);
E.o(F, {}, "/");
E.mkdir("/tmp");
E.mkdir("/home");
E.mkdir("/home/web_user");
(function() {
  E.mkdir("/dev");
  zb(E.ea(1, 3), {read:() => 0, write:(d, e, f, g) => g, D:() => 0});
  cc("/dev/null", E.ea(1, 3));
  yb(E.ea(5, 0), Bb);
  yb(E.ea(6, 0), Cb);
  cc("/dev/tty", E.ea(5, 0));
  cc("/dev/tty1", E.ea(6, 0));
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
  E.o({o() {
    var b = E.createNode(a, "fd", 16895, 73);
    b.j = {D:F.j.D};
    b.h = {lookup(c, d) {
      c = +d;
      var e = Xb(c);
      c = {parent:null, o:{xb:"fake"}, h:{readlink:() => e.path}, id:c + 1};
      return c.parent = c;
    }, readdir() {
      return Array.from(E.streams.entries()).filter(([, c]) => c).map(([c]) => c.toString());
    }};
    return b;
  }}, {}, "/proc/self/fd");
})();
E.Xb = {MEMFS:F};
var Qg = () => {
  if (U) {
    var a = U.oa;
    U.oa = U.ua;
    U.ua = a;
    a = U.da;
    U.da = U.Ga;
    U.Ga = a;
    for (a = 0; 21 >= a; ++a) {
      U.da[a] = 0;
    }
  }
};
"undefined" != typeof xd && Dd.push(Qg);
h.requestAnimationFrame = Kd;
h.pauseMainLoop = Hd;
h.resumeMainLoop = Id;
h.preMainLoop && Dd.push(h.preMainLoop);
h.postMainLoop && Ed.push(h.postMainLoop);
for (let a = 0; 32 > a; ++a) {
  Oe.push(Array(a));
}
for (var Rg = new Float32Array(288), Sg = 0; 288 >= Sg; ++Sg) {
  Of[Sg] = Rg.subarray(0, Sg);
}
var Tg = new Int32Array(288);
for (Sg = 0; 288 >= Sg; ++Sg) {
  Qf[Sg] = Tg.subarray(0, Sg);
}
h.noExitRuntime && (eb = h.noExitRuntime);
h.preloadPlugins && (Kb = h.preloadPlugins);
h.print && (qa = h.print);
h.printErr && (l = h.printErr);
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
  function f(y) {
    k(0 < tc);
    --tc;
    0 !== r && $d(r);
    return "string" === b ? D(y) : "boolean" === b ? !!y : y;
  }
  var g = {string:y => {
    var I = 0;
    null !== y && void 0 !== y && 0 !== y && (I = Zd(y));
    return I;
  }, array:y => {
    var I = Yd(y.length);
    k(0 <= y.length, "writeArrayToMemory array must have a length (should be an array or typed array)");
    w.set(y, I);
    return I;
  }}, m = Pg(a), p = [], r = 0;
  k("array" !== b, 'Return type should not be "array".');
  if (d) {
    for (var t = 0; t < d.length; t++) {
      var v = g[c[t]];
      v ? (0 === r && (r = rc()), p[t] = v(d[t])) : p[t] = d[t];
    }
  }
  c = yg;
  m = m(...p);
  e = e?.async;
  tc += 1;
  if (yg != c) {
    return k(!(c && yg), "We cannot start an async operation when one is already in flight"), k(!(c && !yg), "We cannot stop an async operation in flight"), k(e, `The call to ${a} is running asynchronously. If this was intended, add the async option to the ccall/cwrap call.`), Hg().then(f);
  }
  m = f(m);
  return e ? Promise.resolve(m) : m;
};
h.UTF8ToString = D;
h.createContext = Fc;
h.FS_preloadFile = Pb;
h.FS_unlink = (...a) => E.unlink(...a);
h.FS_createPath = (...a) => gc(...a);
h.FS_createDevice = (...a) => E.$(...a);
h.FS = E;
h.FS_createDataFile = (...a) => Ob(...a);
h.FS_createLazyFile = (...a) => jc(...a);
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
"run out err callMain abort wasmExports writeStackCookie checkStackCookie writeI53ToI64 readI53FromI64 readI53FromU64 INT53_MAX INT53_MIN bigintToI53Checked HEAP8 HEAPU8 HEAP16 HEAPU16 HEAP32 HEAPU32 HEAPF32 HEAPF64 HEAP64 HEAPU64 stackSave stackRestore stackAlloc createNamedFunction ptrToString exitJS getHeapMax growMemory ENV ERRNO_CODES strError DNS Protocols Sockets timers warnOnce readEmAsmArgsArray readEmAsmArgs runEmAsmFunction runMainThreadEmAsm jstoi_q getExecutableName autoResumeAudioContext dynCallLegacy dynCall handleException keepRuntimeAlive runtimeKeepalivePush runtimeKeepalivePop callUserCallback maybeExit asyncLoad alignMemory mmapAlloc wasmTable wasmMemory getUniqueRunDependency noExitRuntime addOnPreRun addOnExit addOnPostRun freeTableIndexes functionsInTableMap setValue getValue PATH PATH_FS UTF8Decoder UTF8ArrayToString stringToUTF8Array stringToUTF8 lengthBytesUTF8 intArrayFromString UTF16Decoder stringToNewUTF8 stringToUTF8OnStack writeArrayToMemory JSEvents registerKeyEventCallback specialHTMLTargets maybeCStringToJsString findEventTarget findCanvasEventTarget getBoundingClientRect fillMouseEventData registerMouseEventCallback registerWheelEventCallback registerUiEventCallback registerFocusEventCallback fillFullscreenChangeEventData registerFullscreenChangeEventCallback JSEvents_requestFullscreen JSEvents_resizeCanvasForFullscreen registerRestoreOldStyle setLetterbox currentFullscreenStrategy restoreOldWindowedStyle doRequestFullscreen fillPointerlockChangeEventData registerPointerlockChangeEventCallback requestPointerLock fillVisibilityChangeEventData registerVisibilityChangeEventCallback registerTouchEventCallback fillGamepadEventData registerGamepadEventCallback registerBeforeUnloadEventCallback setCanvasElementSize getCanvasElementSize UNWIND_CACHE ExitStatus getEnvStrings checkWasiClock doReadv doWritev initRandomFill randomFill safeSetTimeout emSetImmediate emClearImmediate_deps emClearImmediate registerPreMainLoop promiseMap uncaughtExceptionCount exceptionCaught Browser requestFullscreen requestFullScreen setCanvasSize getUserMedia getPreloadedImageData__data wget MONTH_DAYS_REGULAR MONTH_DAYS_LEAP MONTH_DAYS_REGULAR_CUMULATIVE MONTH_DAYS_LEAP_CUMULATIVE isLeapYear ydayFromDate SYSCALLS preloadPlugins FS_createPreloadedFile FS_modeStringToFlags FS_getMode FS_fileDataToTypedArray FS_stdin_getChar_buffer FS_stdin_getChar FS_readFile FS_root FS_mounts FS_devices FS_streams FS_nextInode FS_nameTable FS_currentPath FS_initialized FS_ignorePermissions FS_filesystems FS_syncFSRequests FS_lookupPath FS_getPath FS_hashName FS_hashAddNode FS_hashRemoveNode FS_lookupNode FS_createNode FS_destroyNode FS_isRoot FS_isMountpoint FS_isFile FS_isDir FS_isLink FS_isChrdev FS_isBlkdev FS_isFIFO FS_isSocket FS_flagsToPermissionString FS_nodePermissions FS_mayLookup FS_mayCreate FS_mayDelete FS_mayOpen FS_checkOpExists FS_nextfd FS_getStreamChecked FS_getStream FS_createStream FS_closeStream FS_dupStream FS_doSetAttr FS_chrdev_stream_ops FS_major FS_minor FS_makedev FS_registerDevice FS_getDevice FS_getMounts FS_syncfs FS_mount FS_unmount FS_lookup FS_mknod FS_statfs FS_statfsStream FS_statfsNode FS_create FS_mkdir FS_mkdev FS_symlink FS_rename FS_rmdir FS_readdir FS_readlink FS_stat FS_fstat FS_lstat FS_doChmod FS_chmod FS_lchmod FS_fchmod FS_doChown FS_chown FS_lchown FS_fchown FS_doTruncate FS_truncate FS_ftruncate FS_utime FS_open FS_close FS_isClosed FS_llseek FS_read FS_write FS_mmap FS_msync FS_ioctl FS_writeFile FS_cwd FS_chdir FS_createDefaultDirectories FS_createDefaultDevices FS_createSpecialDirectories FS_createStandardStreams FS_staticInit FS_init FS_quit FS_findObject FS_analyzePath FS_createFile FS_forceLoadFile MEMFS TTY PIPEFS SOCKFS tempFixedLengthArray miniTempWebGLFloatBuffers miniTempWebGLIntBuffers heapObjectForWebGLType toTypedArrayIndex webgl_enable_ANGLE_instanced_arrays webgl_enable_OES_vertex_array_object webgl_enable_WEBGL_draw_buffers webgl_enable_WEBGL_multi_draw webgl_enable_EXT_polygon_offset_clamp webgl_enable_EXT_clip_control webgl_enable_WEBGL_polygon_mode GL emscriptenWebGLGet computeUnpackAlignedImageSize colorChannelsInGlTextureFormat emscriptenWebGLGetTexPixelData emscriptenWebGLGetUniform webglGetUniformLocation webglPrepareUniformLocationsBeforeFirstUse webglGetLeftBracePos emscriptenWebGLGetVertexAttrib __glGetActiveAttribOrUniform emscriptenWebGLGetBufferBinding emscriptenWebGLValidateMapBufferTarget AL GLUT EGL GLEW IDBStore runAndAbortIfError Asyncify Fibers emscriptenWebGLGetIndexed webgl_enable_WEBGL_draw_instanced_base_vertex_base_instance webgl_enable_WEBGL_multi_draw_instanced_base_vertex_base_instance print printErr jstoi_s".split(" ").forEach(Ga);
var Od = {155596:() => h.canvas.width, 155628:() => h.canvas.height, 155661:() => {
  window.print();
}, 155676:a => {
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
}, 156401:a => {
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
      } catch (m) {
      }
      var g = "/tmp/" + e.name;
      E.writeFile(g, f);
      h.Ia("web_on_file_ready", null, ["string"], [g]);
    }, d.onerror = function() {
      b();
      h.Ia("web_on_file_cancelled", null, [], []);
    }, d.readAsArrayBuffer(e)) : h.Ia("web_on_file_cancelled", null, [], []);
  };
  c.addEventListener("cancel", function() {
    b();
    h.Ia("web_on_file_cancelled", null, [], []);
  });
  c.click();
}, 157334:() => "undefined" !== typeof AudioContext || "undefined" !== typeof webkitAudioContext ? !0 : !1, 157481:() => "undefined" !== typeof navigator.mediaDevices && "undefined" !== typeof navigator.mediaDevices.getUserMedia || "undefined" !== typeof navigator.webkitGetUserMedia ? !0 : !1, 157715:a => {
  "undefined" === typeof h.SDL2 && (h.SDL2 = {});
  var b = h.SDL2;
  a ? b.capture = {} : b.audio = {};
  b.m || ("undefined" !== typeof AudioContext ? b.m = new AudioContext() : "undefined" !== typeof webkitAudioContext && (b.m = new webkitAudioContext()), b.m && "undefined" === typeof navigator.userActivation && pg(b.m));
  return void 0 === b.m ? -1 : 0;
}, 158267:() => h.SDL2.m.sampleRate, 158335:(a, b, c, d) => {
  function e() {
  }
  function f(m) {
    void 0 !== g.capture.R && (clearInterval(g.capture.R), g.capture.R = void 0, g.capture.ga = void 0);
    g.capture.Xa = g.m.createMediaStreamSource(m);
    g.capture.O = g.m.createScriptProcessor(b, a, 1);
    g.capture.O.onaudioprocess = function(p) {
      void 0 !== g && void 0 !== g.capture && (p.outputBuffer.getChannelData(0).fill(0.0), g.capture.Ja = p.inputBuffer, db(c, [d]));
    };
    g.capture.Xa.connect(g.capture.O);
    g.capture.O.connect(g.m.destination);
    g.capture.stream = m;
  }
  var g = h.SDL2;
  g.capture.ga = g.m.createBuffer(a, b, g.m.sampleRate);
  g.capture.ga.getChannelData(0).fill(0.0);
  g.capture.R = setInterval(function() {
    g.capture.Ja = g.capture.ga;
    db(c, [d]);
  }, b / g.m.sampleRate * 1000);
  void 0 !== navigator.mediaDevices && void 0 !== navigator.mediaDevices.getUserMedia ? navigator.mediaDevices.getUserMedia({audio:!0, video:!1}).then(f).catch(e) : void 0 !== navigator.webkitGetUserMedia && navigator.webkitGetUserMedia({audio:!0, video:!1}, f, e);
}, 160028:(a, b, c, d) => {
  var e = h.SDL2;
  e.audio.O = e.m.createScriptProcessor(b, 0, a);
  e.audio.O.onaudioprocess = function(f) {
    void 0 !== e && void 0 !== e.audio && (void 0 !== e.audio.R && (clearInterval(e.audio.R), e.audio.R = void 0, e.audio.ga = void 0), e.audio.za = f.outputBuffer, db(c, [d]));
  };
  e.audio.O.connect(e.m.destination);
  "suspended" === e.m.state && (e.audio.ga = e.m.createBuffer(a, b, e.m.sampleRate), e.audio.ga.getChannelData(0).fill(0.0), e.audio.R = setInterval(function() {
    "undefined" !== typeof navigator.userActivation && navigator.userActivation.hasBeenActive && e.m.resume();
    e.audio.za = e.audio.ga;
    db(c, [d]);
    e.audio.za = void 0;
  }, b / e.m.sampleRate * 1000));
}, 161203:(a, b) => {
  for (var c = h.SDL2, d = c.capture.Ja.numberOfChannels, e = 0; e < d; ++e) {
    var f = c.capture.Ja.getChannelData(e);
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
}, 161808:(a, b) => {
  var c = h.SDL2;
  a >>>= 2;
  for (var d = c.audio.za.numberOfChannels, e = 0; e < d; ++e) {
    var f = c.audio.za.getChannelData(e);
    if (f.length != b) {
      throw "Web Audio output buffer length mismatch! Destination size: " + f.length + " samples vs expected " + b + " samples!";
    }
    for (var g = 0; g < b; ++g) {
      f[g] = A[a + (g * d + e)];
    }
  }
}, 162297:a => {
  var b = h.SDL2;
  if (a) {
    void 0 !== b.capture.R && clearInterval(b.capture.R);
    if (void 0 !== b.capture.stream) {
      a = b.capture.stream.getAudioTracks();
      for (var c = 0; c < a.length; c++) {
        b.capture.stream.removeTrack(a[c]);
      }
    }
    void 0 !== b.capture.O && (b.capture.O.onaudioprocess = function() {
    }, b.capture.O.disconnect());
    void 0 !== b.capture.Xa && b.capture.Xa.disconnect();
    b.capture = void 0;
  } else {
    void 0 != b.audio.O && b.audio.O.disconnect(), void 0 !== b.audio.R && clearInterval(b.audio.R), b.audio = void 0;
  }
  void 0 !== b.m && void 0 === b.audio && void 0 === b.capture && (b.m.close(), b.m = void 0);
}, 163303:(a, b, c) => {
  h.SDL2 || (h.SDL2 = {});
  var d = h.SDL2;
  d.Lb !== h.canvas && (d.ya = Fc(h.canvas, !1, !0), d.Lb = h.canvas);
  if (d.w !== a || d.ac !== b || d.bc !== d.ya) {
    d.image = d.ya.createImageData(a, b), d.w = a, d.ac = b, d.bc = d.ya;
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
    if (d.Pb !== a && (d.Ob = new Int32Array(a.buffer), d.Qb = new Uint8Array(a.buffer), d.Pb = a), a = d.Ob, c = a.length, a.set(z.subarray(b, b + c)), a = d.Qb, b = 3, e = b + 4 * c, 0 == c % 8) {
      for (; b < e;) {
        a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0, a[b] = 255, b = b + 4 | 0;
      }
    } else {
      for (; b < e;) {
        a[b] = 255, b = b + 4 | 0;
      }
    }
  }
  d.ya.putImageData(d.image, 0, 0);
}, 164769:(a, b, c, d, e) => {
  var f = document.createElement("canvas");
  f.width = a;
  f.height = b;
  var g = f.getContext("2d");
  a = g.createImageData(a, b);
  b = a.data;
  e /= 4;
  var m = 0, p;
  if ("undefined" !== typeof CanvasPixelArray && b instanceof CanvasPixelArray) {
    for (p = b.length; m < p;) {
      var r = z[e];
      b[m] = r & 255;
      b[m + 1] = r >> 8 & 255;
      b[m + 2] = r >> 16 & 255;
      b[m + 3] = r >> 24 & 255;
      e++;
      m += 4;
    }
  } else {
    b = new Int32Array(b.buffer), p = b.length, b.set(z.subarray(e, e + p));
  }
  g.putImageData(a, 0, 0);
  c = 0 === c && 0 === d ? "url(" + f.toDataURL() + "), auto" : "url(" + f.toDataURL() + ") " + c + " " + d + ", auto";
  d = W(c.length + 1);
  J(c, d, c.length + 1);
  return d;
}, 165757:a => {
  h.canvas && (h.canvas.style.cursor = D(a));
}, 165840:() => {
  h.canvas && (h.canvas.style.cursor = "none");
}, 165909:() => window.innerWidth, 165939:() => window.innerHeight}, Ug = h._main = u("_main"), Og = u("_free"), W = u("_malloc"), lc = u("_fflush");
h._web_on_file_ready = u("_web_on_file_ready");
h._web_on_file_cancelled = u("_web_on_file_cancelled");
var kc = u("_strerror"), va = u("_emscripten_stack_get_end"), Vg = u("_emscripten_stack_init"), $d = u("__emscripten_stack_restore"), Yd = u("__emscripten_stack_alloc"), rc = u("_emscripten_stack_get_current"), dynCall_v = u("dynCall_v"), ee = u("dynCall_iiii"), dynCall_vii = u("dynCall_vii"), dynCall_vi = u("dynCall_vi"), dynCall_iii = u("dynCall_iii"), Mg = u("_asyncify_start_unwind"), zg = u("_asyncify_stop_unwind"), Lg = u("_asyncify_start_rewind"), Ng = u("_asyncify_stop_rewind"), Ja = u("wasmMemory"), 
sg = {__assert_fail:(a, b, c, d) => n(`Assertion failed: ${D(a)}, at: ` + [b ? D(b) : "unknown filename", c, d ? D(d) : "unknown function"]), __syscall_fcntl64:function(a, b, c) {
  ib = c;
  try {
    var d = Xb(a);
    switch(b) {
      case 0:
        var e = jb();
        if (0 > e) {
          break;
        }
        for (; E.streams[e];) {
          e++;
        }
        return Zb(d, e).fd;
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
    return nc(b, E.fstat(a));
  } catch (c) {
    if ("undefined" == typeof E || "ErrnoError" !== c.name) {
      throw c;
    }
    return -c.s;
  }
}, __syscall_ioctl:function(a, b, c) {
  ib = c;
  try {
    var d = Xb(a);
    switch(b) {
      case 21509:
        return d.tty ? 0 : -59;
      case 21505:
        if (!d.tty) {
          return -59;
        }
        if (d.tty.Z.dc) {
          a = [3, 28, 127, 21, 4, 0, 1, 0, 17, 19, 26, 0, 18, 15, 23, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
          var e = jb();
          z[e >> 2] = 25856;
          z[e + 4 >> 2] = 5;
          z[e + 8 >> 2] = 191;
          z[e + 12 >> 2] = 35387;
          for (var f = 0; 32 > f; f++) {
            w[e + f + 17] = a[f] || 0;
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
        if (d.tty.Z.ec) {
          for (e = jb(), a = [], f = 0; 32 > f; f++) {
            a.push(w[e + f + 17]);
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
        return e = jb(), E.Qa(d, b, e);
      case 21523:
        if (!d.tty) {
          return -59;
        }
        d.tty.Z.fc && (f = [24, 80], e = jb(), Ka[e >> 1] = f[0], Ka[e + 2 >> 1] = f[1]);
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
    return a = D(a), nc(b, E.lstat(a));
  } catch (c) {
    if ("undefined" == typeof E || "ErrnoError" !== c.name) {
      throw c;
    }
    return -c.s;
  }
}, __syscall_mkdirat:function(a, b, c) {
  try {
    return b = D(b), b = mc(a, b), E.mkdir(b, c, 0), 0;
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
    b = mc(a, b, f);
    return nc(c, e ? E.lstat(b) : E.stat(b));
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
    b = mc(a, b);
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
    return a = D(a), nc(b, E.stat(a));
  } catch (c) {
    if ("undefined" == typeof E || "ErrnoError" !== c.name) {
      throw c;
    }
    return -c.s;
  }
}, _abort_js:() => n("native code called abort()"), _localtime_js:function(a, b) {
  a = -9007199254740992 > a || 9007199254740992 < a ? NaN : Number(a);
  a = new Date(1000 * a);
  z[b >> 2] = a.getSeconds();
  z[b + 4 >> 2] = a.getMinutes();
  z[b + 8 >> 2] = a.getHours();
  z[b + 12 >> 2] = a.getDate();
  z[b + 16 >> 2] = a.getMonth();
  z[b + 20 >> 2] = a.getFullYear() - 1900;
  z[b + 24 >> 2] = a.getDay();
  z[b + 28 >> 2] = (oc(a.getFullYear()) ? pc : qc)[a.getMonth()] + a.getDate() - 1 | 0;
  z[b + 36 >> 2] = -(60 * a.getTimezoneOffset());
  var c = (new Date(a.getFullYear(), 6, 1)).getTimezoneOffset(), d = (new Date(a.getFullYear(), 0, 1)).getTimezoneOffset();
  z[b + 32 >> 2] = (c != d && a.getTimezoneOffset() == Math.min(d, c)) | 0;
}, _mktime_js:function(a) {
  var b = new Date(z[a + 20 >> 2] + 1900, z[a + 16 >> 2], z[a + 12 >> 2], z[a + 8 >> 2], z[a + 4 >> 2], z[a >> 2], 0), c = z[a + 32 >> 2], d = b.getTimezoneOffset(), e = (new Date(b.getFullYear(), 6, 1)).getTimezoneOffset(), f = (new Date(b.getFullYear(), 0, 1)).getTimezoneOffset(), g = Math.min(f, e);
  0 > c ? z[a + 32 >> 2] = Number(e != f && g == d) : 0 < c != (g == d) && (e = Math.max(f, e), b.setTime(b.getTime() + 60000 * ((0 < c ? g : e) - d)));
  z[a + 24 >> 2] = b.getDay();
  z[a + 28 >> 2] = (oc(b.getFullYear()) ? pc : qc)[b.getMonth()] + b.getDate() - 1 | 0;
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
    var m = Math.abs(g);
    return `UTC${0 <= g ? "-" : "+"}${String(Math.floor(m / 60)).padStart(2, "0")}${String(m % 60).padStart(2, "0")}`;
  };
  a = b(f);
  b = b(e);
  k(a);
  k(b);
  k(16 >= ub(a), `timezone name truncated to fit in TZNAME_MAX (${a})`);
  k(16 >= ub(b), `timezone name truncated to fit in TZNAME_MAX (${b})`);
  e < f ? (J(a, c, 17), J(b, d, 17)) : (J(a, d, 17), J(b, c, 17));
}, clock_time_get:function(a, b, c) {
  if (!(0 <= a && 3 >= a)) {
    return 28;
  }
  Na[c >> 3] = BigInt(Math.round(1E6 * (0 === a ? Date.now() : performance.now())));
  return 0;
}, eglBindAPI:a => {
  if (12448 == a) {
    return L = 12288, 1;
  }
  L = 12300;
  return 0;
}, eglChooseConfig:(a, b, c, d, e) => {
  if (62000 != a) {
    L = 12296, c = 0;
  } else {
    if (b) {
      for (;;) {
        a = z[b >> 2];
        if (12321 == a) {
          M.alpha = 0 < z[b + 4 >> 2];
        } else if (12325 == a) {
          M.depth = 0 < z[b + 4 >> 2];
        } else if (12326 == a) {
          M.stencil = 0 < z[b + 4 >> 2];
        } else if (12337 == a) {
          a = z[b + 4 >> 2], M.antialias = 0 < a;
        } else if (12338 == a) {
          a = z[b + 4 >> 2], M.antialias = 1 == a;
        } else if (12544 == a) {
          M.Vc = 12547 != z[b + 4 >> 2];
        } else if (12344 == a) {
          break;
        }
        b += 8;
      }
    }
    c && d || e ? (e && (z[e >> 2] = 1), c && 0 < d && (q[c >> 2] = 62002), L = 12288, c = 1) : (L = 12300, c = 0);
  }
  return c;
}, eglCreateContext:(a, b, c, d) => {
  if (62000 != a) {
    return L = 12296, 0;
  }
  for (a = 1;;) {
    b = z[d >> 2];
    if (12440 == b) {
      a = z[d + 4 >> 2];
    } else if (12344 == b) {
      break;
    } else {
      return L = 12292, 0;
    }
    d += 8;
  }
  if (2 > a || 3 < a) {
    return L = 12293, 0;
  }
  M.Va = a - 1;
  M.Yc = 0;
  Pc = Hc(h.canvas, M);
  if (0 != Pc) {
    return L = 12288, Jc(Pc), Cc.forEach(e => e()), Jc(null), 62004;
  }
  L = 12297;
  return 0;
}, eglCreateWindowSurface:(a, b) => {
  if (62000 != a) {
    return L = 12296, 0;
  }
  if (62002 != b) {
    return L = 12293, 0;
  }
  L = 12288;
  return 62006;
}, eglDestroyContext:(a, b) => {
  if (62000 != a) {
    return L = 12296, 0;
  }
  if (62004 != b) {
    return L = 12294, 0;
  }
  a = Pc;
  U === Ic[a] && (U = null);
  "object" == typeof X && X.qc(Ic[a].U.canvas);
  Ic[a]?.U.canvas && (Ic[a].U.canvas.pa = void 0);
  Ic[a] = null;
  L = 12288;
  Lc == b && (Lc = 0);
  return 1;
}, eglDestroySurface:(a, b) => {
  if (62000 != a) {
    return L = 12296, 0;
  }
  if (62006 != b) {
    return L = 12301, 1;
  }
  Mc == b && (Mc = 0);
  Nc == b && (Nc = 0);
  L = 12288;
  return 1;
}, eglGetConfigAttrib:(a, b, c, d) => {
  if (62000 != a) {
    return L = 12296, 0;
  }
  if (62002 != b) {
    return L = 12293, 0;
  }
  if (!d) {
    return L = 12300, 0;
  }
  L = 12288;
  switch(c) {
    case 12320:
      return z[d >> 2] = M.alpha ? 32 : 24, 1;
    case 12321:
      return z[d >> 2] = M.alpha ? 8 : 0, 1;
    case 12322:
      return z[d >> 2] = 8, 1;
    case 12323:
      return z[d >> 2] = 8, 1;
    case 12324:
      return z[d >> 2] = 8, 1;
    case 12325:
      return z[d >> 2] = M.depth ? 24 : 0, 1;
    case 12326:
      return z[d >> 2] = M.stencil ? 8 : 0, 1;
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
      return z[d >> 2] = M.antialias ? 4 : 0, 1;
    case 12338:
      return z[d >> 2] = M.antialias ? 1 : 0, 1;
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
      return L = 12292, 0;
  }
}, eglGetDisplay:a => {
  L = 12288;
  return 0 != a && 1 != a ? 0 : 62000;
}, eglGetError:() => L, eglInitialize:(a, b, c) => {
  if (62000 != a) {
    return L = 12296, 0;
  }
  b && (z[b >> 2] = 1);
  c && (z[c >> 2] = 4);
  Kc = !0;
  L = 12288;
  return 1;
}, eglMakeCurrent:(a, b, c, d) => {
  if (62000 != a) {
    return L = 12296, 0;
  }
  if (0 != d && 62004 != d) {
    return L = 12294, 0;
  }
  if (0 != c && 62006 != c || 0 != b && 62006 != b) {
    return L = 12301, 0;
  }
  Jc(d ? Pc : null);
  Lc = d;
  Nc = b;
  Mc = c;
  L = 12288;
  return 1;
}, eglQueryString:(a, b) => {
  if (62000 != a) {
    return L = 12296, 0;
  }
  L = 12288;
  if (Oc[b]) {
    return Oc[b];
  }
  switch(b) {
    case 12371:
      a = pd("Emscripten");
      break;
    case 12372:
      a = pd("1.4 Emscripten EGL");
      break;
    case 12373:
      a = pd("");
      break;
    case 12429:
      a = pd("OpenGL_ES");
      break;
    default:
      return L = 12300, 0;
  }
  return Oc[b] = a;
}, eglSwapBuffers:() => {
  if (Kc) {
    if (K) {
      if (K.isContextLost()) {
        L = 12302;
      } else {
        return L = 12288, 1;
      }
    } else {
      L = 12290;
    }
  } else {
    L = 12289;
  }
  return 0;
}, eglSwapInterval:(a, b) => {
  if (62000 != a) {
    return L = 12296, 0;
  }
  0 == b ? Fd(0, 0) : Fd(1, b);
  L = 12288;
  return 1;
}, eglTerminate:a => {
  if (62000 != a) {
    return L = 12296, 0;
  }
  Nc = Mc = Lc = 0;
  Kc = !1;
  L = 12288;
  return 1;
}, eglWaitGL:() => {
  L = 12288;
  return 1;
}, eglWaitNative:() => {
  L = 12288;
  return 1;
}, emscripten_asm_const_int:(a, b, c) => {
  b = Nd(b, c);
  k(Od.hasOwnProperty(a), `No EM_ASM constant found at address ${a}.  The loaded WebAssembly file is likely out of sync with the generated JavaScript.`);
  return Od[a](...b);
}, emscripten_asm_const_int_sync_on_main_thread:(a, b, c) => Pd(a, b, c), emscripten_asm_const_ptr_sync_on_main_thread:(a, b, c) => Pd(a, b, c), emscripten_date_now:() => Date.now(), emscripten_exit_fullscreen:() => {
  if (!X.fullscreenEnabled()) {
    return -1;
  }
  Sd(ie);
  var a = Xd[1];
  if (a.exitFullscreen) {
    a.fullscreenElement && a.exitFullscreen();
  } else if (a.webkitExitFullscreen) {
    a.webkitFullscreenElement && a.webkitExitFullscreen();
  } else {
    return -1;
  }
  return 0;
}, emscripten_exit_pointerlock:() => {
  Sd(je);
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
  a = he(a);
  Ma[b >> 3] = a.width;
  Ma[c >> 3] = a.height;
  return 0;
}, emscripten_get_gamepad_status:(a, b) => {
  k(X.ka, "emscripten_get_gamepad_status() can only be called after having first called emscripten_sample_gamepad_data() and that function has returned EMSCRIPTEN_RESULT_SUCCESS!");
  if (0 > a || a >= X.ka.length) {
    return -5;
  }
  if (!X.ka[a]) {
    return -7;
  }
  ke(b, X.ka[a]);
  return 0;
}, emscripten_get_now:() => performance.now(), emscripten_get_num_gamepads:() => {
  k(X.ka, "emscripten_get_num_gamepads() can only be called after having first called emscripten_sample_gamepad_data() and that function has returned EMSCRIPTEN_RESULT_SUCCESS!");
  return X.ka.length;
}, emscripten_get_screen_size:(a, b) => {
  z[a >> 2] = screen.width;
  z[b >> 2] = screen.height;
}, emscripten_glActiveTexture:le, emscripten_glAttachShader:me, emscripten_glBeginQuery:(a, b) => {
  K.beginQuery(a, P[b]);
}, emscripten_glBeginQueryEXT:(a, b) => {
  K.J.beginQueryEXT(a, P[b]);
}, emscripten_glBeginTransformFeedback:a => K.beginTransformFeedback(a), emscripten_glBindAttribLocation:ne, emscripten_glBindBuffer:oe, emscripten_glBindBufferBase:(a, b, c) => {
  K.bindBufferBase(a, b, Vc[c]);
}, emscripten_glBindBufferRange:(a, b, c, d, e) => {
  K.bindBufferRange(a, b, Vc[c], d, e);
}, emscripten_glBindFramebuffer:pe, emscripten_glBindRenderbuffer:qe, emscripten_glBindSampler:(a, b) => {
  K.bindSampler(a, ad[b]);
}, emscripten_glBindTexture:re, emscripten_glBindTransformFeedback:(a, b) => {
  K.bindTransformFeedback(a, bd[b]);
}, emscripten_glBindVertexArray:se, emscripten_glBindVertexArrayOES:se, emscripten_glBlendColor:(a, b, c, d) => K.blendColor(a, b, c, d), emscripten_glBlendEquation:a => K.blendEquation(a), emscripten_glBlendEquationSeparate:(a, b) => K.blendEquationSeparate(a, b), emscripten_glBlendFunc:(a, b) => K.blendFunc(a, b), emscripten_glBlendFuncSeparate:te, emscripten_glBlitFramebuffer:(a, b, c, d, e, f, g, m, p, r) => K.blitFramebuffer(a, b, c, d, e, f, g, m, p, r), emscripten_glBufferData:ue, emscripten_glBufferSubData:(a, 
b, c, d) => {
  2 <= U.version ? c && K.bufferSubData(a, b, x, d, c) : K.bufferSubData(a, b, x.subarray(d, d + c));
}, emscripten_glCheckFramebufferStatus:ve, emscripten_glClear:we, emscripten_glClearBufferfi:(a, b, c, d) => K.clearBufferfi(a, b, c, d), emscripten_glClearBufferfv:(a, b, c) => {
  K.clearBufferfv(a, b, A, c >> 2);
}, emscripten_glClearBufferiv:(a, b, c) => {
  K.clearBufferiv(a, b, z, c >> 2);
}, emscripten_glClearBufferuiv:(a, b, c) => {
  K.clearBufferuiv(a, b, q, c >> 2);
}, emscripten_glClearColor:xe, emscripten_glClearDepthf:a => K.clearDepth(a), emscripten_glClearStencil:a => K.clearStencil(a), emscripten_glClientWaitSync:(a, b, c) => {
  c = Number(c);
  return K.clientWaitSync(cd[a], b, c);
}, emscripten_glClipControlEXT:(a, b) => {
  K.Vb.clipControlEXT(a, b);
}, emscripten_glColorMask:ye, emscripten_glCompileShader:ze, emscripten_glCompressedTexImage2D:(a, b, c, d, e, f, g, m) => {
  2 <= U.version ? K.S || !g ? K.compressedTexImage2D(a, b, c, d, e, f, g, m) : K.compressedTexImage2D(a, b, c, d, e, f, x, m, g) : K.compressedTexImage2D(a, b, c, d, e, f, x.subarray(m, m + g));
}, emscripten_glCompressedTexImage3D:(a, b, c, d, e, f, g, m, p) => {
  K.S ? K.compressedTexImage3D(a, b, c, d, e, f, g, m, p) : K.compressedTexImage3D(a, b, c, d, e, f, g, x, p, m);
}, emscripten_glCompressedTexSubImage2D:(a, b, c, d, e, f, g, m, p) => {
  2 <= U.version ? K.S || !m ? K.compressedTexSubImage2D(a, b, c, d, e, f, g, m, p) : K.compressedTexSubImage2D(a, b, c, d, e, f, g, x, p, m) : K.compressedTexSubImage2D(a, b, c, d, e, f, g, x.subarray(p, p + m));
}, emscripten_glCompressedTexSubImage3D:(a, b, c, d, e, f, g, m, p, r, t) => {
  K.S ? K.compressedTexSubImage3D(a, b, c, d, e, f, g, m, p, r, t) : K.compressedTexSubImage3D(a, b, c, d, e, f, g, m, p, x, t, r);
}, emscripten_glCopyBufferSubData:(a, b, c, d, e) => K.copyBufferSubData(a, b, c, d, e), emscripten_glCopyTexImage2D:(a, b, c, d, e, f, g, m) => K.copyTexImage2D(a, b, c, d, e, f, g, m), emscripten_glCopyTexSubImage2D:(a, b, c, d, e, f, g, m) => K.copyTexSubImage2D(a, b, c, d, e, f, g, m), emscripten_glCopyTexSubImage3D:(a, b, c, d, e, f, g, m, p) => K.copyTexSubImage3D(a, b, c, d, e, f, g, m, p), emscripten_glCreateProgram:Ae, emscripten_glCreateShader:Be, emscripten_glCullFace:Ce, emscripten_glDeleteBuffers:De, 
emscripten_glDeleteFramebuffers:Ee, emscripten_glDeleteProgram:Fe, emscripten_glDeleteQueries:(a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = P[d];
    e && (K.deleteQuery(e), P[d] = null);
  }
}, emscripten_glDeleteQueriesEXT:(a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = P[d];
    e && (K.J.deleteQueryEXT(e), P[d] = null);
  }
}, emscripten_glDeleteRenderbuffers:Ge, emscripten_glDeleteSamplers:(a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = ad[d];
    e && (K.deleteSampler(e), e.name = 0, ad[d] = null);
  }
}, emscripten_glDeleteShader:He, emscripten_glDeleteSync:a => {
  if (a) {
    var b = cd[a];
    b ? (K.deleteSync(b), b.name = 0, cd[a] = null) : T ||= 1281;
  }
}, emscripten_glDeleteTextures:Ie, emscripten_glDeleteTransformFeedbacks:(a, b) => {
  for (var c = 0; c < a; c++) {
    var d = z[b + 4 * c >> 2], e = bd[d];
    e && (K.deleteTransformFeedback(e), e.name = 0, bd[d] = null);
  }
}, emscripten_glDeleteVertexArrays:Je, emscripten_glDeleteVertexArraysOES:Je, emscripten_glDepthFunc:a => K.depthFunc(a), emscripten_glDepthMask:a => {
  K.depthMask(!!a);
}, emscripten_glDepthRangef:(a, b) => K.depthRange(a, b), emscripten_glDetachShader:(a, b) => {
  K.detachShader(N[a], O[b]);
}, emscripten_glDisable:Ke, emscripten_glDisableVertexAttribArray:Le, emscripten_glDrawArrays:Me, emscripten_glDrawArraysInstanced:Ne, emscripten_glDrawArraysInstancedANGLE:Ne, emscripten_glDrawArraysInstancedARB:Ne, emscripten_glDrawArraysInstancedEXT:Ne, emscripten_glDrawArraysInstancedNV:Ne, emscripten_glDrawBuffers:Pe, emscripten_glDrawBuffersEXT:Pe, emscripten_glDrawBuffersWEBGL:Pe, emscripten_glDrawElements:Qe, emscripten_glDrawElementsInstanced:Re, emscripten_glDrawElementsInstancedANGLE:Re, 
emscripten_glDrawElementsInstancedARB:Re, emscripten_glDrawElementsInstancedEXT:Re, emscripten_glDrawElementsInstancedNV:Re, emscripten_glDrawRangeElements:(a, b, c, d, e, f) => {
  Qe(a, d, e, f);
}, emscripten_glEnable:Se, emscripten_glEnableVertexAttribArray:Te, emscripten_glEndQuery:a => K.endQuery(a), emscripten_glEndQueryEXT:a => {
  K.J.endQueryEXT(a);
}, emscripten_glEndTransformFeedback:() => K.endTransformFeedback(), emscripten_glFenceSync:(a, b) => (a = K.fenceSync(a, b)) ? (b = jd(cd), a.name = b, cd[b] = a, b) : 0, emscripten_glFinish:Ue, emscripten_glFlush:() => K.flush(), emscripten_glFlushMappedBufferRange:(a, b, c) => {
  if (We(a)) {
    var d = Wc[Ve(a)];
    d ? d.lb & 16 ? 0 > b || 0 > c || b + c > d.length ? (T ||= 1281, l("invalid range in glFlushMappedBufferRange")) : K.bufferSubData(a, d.offset, x.subarray(d.W + b, d.W + b + c)) : (T ||= 1282, l("buffer was not mapped with GL_MAP_FLUSH_EXPLICIT_BIT in glFlushMappedBufferRange")) : (T ||= 1282, l("buffer was never mapped in glFlushMappedBufferRange"));
  } else {
    T ||= 1280, l("GL_INVALID_ENUM in glFlushMappedBufferRange");
  }
}, emscripten_glFramebufferRenderbuffer:Xe, emscripten_glFramebufferTexture2D:Ye, emscripten_glFramebufferTextureLayer:(a, b, c, d, e) => {
  K.framebufferTextureLayer(a, b, Zc[c], d, e);
}, emscripten_glFrontFace:Ze, emscripten_glGenBuffers:$e, emscripten_glGenFramebuffers:af, emscripten_glGenQueries:(a, b) => {
  kd(a, b, "createQuery", P);
}, emscripten_glGenQueriesEXT:(a, b) => {
  for (var c = 0; c < a; c++) {
    var d = K.J.createQueryEXT();
    if (!d) {
      for (T ||= 1282; c < a;) {
        z[b + 4 * c++ >> 2] = 0;
      }
      break;
    }
    var e = jd(P);
    d.name = e;
    P[e] = d;
    z[b + 4 * c >> 2] = e;
  }
}, emscripten_glGenRenderbuffers:bf, emscripten_glGenSamplers:(a, b) => {
  kd(a, b, "createSampler", ad);
}, emscripten_glGenTextures:cf, emscripten_glGenTransformFeedbacks:(a, b) => {
  kd(a, b, "createTransformFeedback", bd);
}, emscripten_glGenVertexArrays:df, emscripten_glGenVertexArraysOES:df, emscripten_glGenerateMipmap:ef, emscripten_glGetActiveAttrib:(a, b, c, d, e, f, g) => ff("getActiveAttrib", a, b, c, d, e, f, g), emscripten_glGetActiveUniform:(a, b, c, d, e, f, g) => ff("getActiveUniform", a, b, c, d, e, f, g), emscripten_glGetActiveUniformBlockName:(a, b, c, d, e) => {
  a = N[a];
  if (a = K.getActiveUniformBlockName(a, b)) {
    e && 0 < c ? (c = J(a, e, c), d && (z[d >> 2] = c)) : d && (z[d >> 2] = 0);
  }
}, emscripten_glGetActiveUniformBlockiv:(a, b, c, d) => {
  if (d) {
    if (a = N[a], 35393 == c) {
      z[d >> 2] = K.getActiveUniformBlockName(a, b).length + 1;
    } else {
      if (a = K.getActiveUniformBlockParameter(a, b, c), null !== a) {
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
      a = N[a];
      for (var f = [], g = 0; g < b; g++) {
        f.push(z[c + 4 * g >> 2]);
      }
      if (a = K.getActiveUniforms(a, f, d)) {
        for (b = a.length, g = 0; g < b; g++) {
          z[e + 4 * g >> 2] = a[g];
        }
      }
    }
  } else {
    T ||= 1281;
  }
}, emscripten_glGetAttachedShaders:(a, b, c, d) => {
  a = K.getAttachedShaders(N[a]);
  var e = a.length;
  e > b && (e = b);
  z[c >> 2] = e;
  for (b = 0; b < e; ++b) {
    z[d + 4 * b >> 2] = O.indexOf(a[b]);
  }
}, emscripten_glGetAttribLocation:(a, b) => K.getAttribLocation(N[a], D(b)), emscripten_glGetBooleanv:(a, b) => jf(a, b, 4), emscripten_glGetBufferParameteri64v:(a, b, c) => {
  c ? gf(c, K.getBufferParameter(a, b)) : T ||= 1281;
}, emscripten_glGetBufferParameteriv:(a, b, c) => {
  c ? z[c >> 2] = K.getBufferParameter(a, b) : T ||= 1281;
}, emscripten_glGetBufferPointerv:(a, b, c) => {
  if (35005 == b) {
    b = 0;
    if (a = Wc[Ve(a)]) {
      b = a.W;
    }
    z[c >> 2] = b;
  } else {
    T ||= 1280, l("GL_INVALID_ENUM in glGetBufferPointerv");
  }
}, emscripten_glGetError:kf, emscripten_glGetFloatv:(a, b) => jf(a, b, 2), emscripten_glGetFragDataLocation:(a, b) => K.getFragDataLocation(N[a], D(b)), emscripten_glGetFramebufferAttachmentParameteriv:(a, b, c, d) => {
  a = K.getFramebufferAttachmentParameter(a, b, c);
  if (a instanceof WebGLRenderbuffer || a instanceof WebGLTexture) {
    a = a.name | 0;
  }
  z[d >> 2] = a;
}, emscripten_glGetInteger64i_v:(a, b, c) => lf(a, b, c, 1), emscripten_glGetInteger64v:(a, b) => {
  jf(a, b, 1);
}, emscripten_glGetIntegeri_v:(a, b, c) => lf(a, b, c, 0), emscripten_glGetIntegerv:mf, emscripten_glGetInternalformativ:(a, b, c, d, e) => {
  if (0 > d) {
    T ||= 1281;
  } else {
    if (e) {
      if (a = K.getInternalformatParameter(a, b, c), null !== a) {
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
}, emscripten_glGetProgramInfoLog:nf, emscripten_glGetProgramiv:of, emscripten_glGetQueryObjecti64vEXT:pf, emscripten_glGetQueryObjectivEXT:qf, emscripten_glGetQueryObjectui64vEXT:pf, emscripten_glGetQueryObjectuiv:(a, b, c) => {
  if (c) {
    a = K.getQueryParameter(P[a], b);
    var d;
    "boolean" == typeof a ? d = a ? 1 : 0 : d = a;
    z[c >> 2] = d;
  } else {
    T ||= 1281;
  }
}, emscripten_glGetQueryObjectuivEXT:qf, emscripten_glGetQueryiv:(a, b, c) => {
  c ? z[c >> 2] = K.getQuery(a, b) : T ||= 1281;
}, emscripten_glGetQueryivEXT:(a, b, c) => {
  c ? z[c >> 2] = K.J.getQueryEXT(a, b) : T ||= 1281;
}, emscripten_glGetRenderbufferParameteriv:(a, b, c) => {
  c ? z[c >> 2] = K.getRenderbufferParameter(a, b) : T ||= 1281;
}, emscripten_glGetSamplerParameterfv:(a, b, c) => {
  c ? A[c >> 2] = K.getSamplerParameter(ad[a], b) : T ||= 1281;
}, emscripten_glGetSamplerParameteriv:(a, b, c) => {
  c ? z[c >> 2] = K.getSamplerParameter(ad[a], b) : T ||= 1281;
}, emscripten_glGetShaderInfoLog:rf, emscripten_glGetShaderPrecisionFormat:(a, b, c, d) => {
  a = K.getShaderPrecisionFormat(a, b);
  z[c >> 2] = a.rangeMin;
  z[c + 4 >> 2] = a.rangeMax;
  z[d >> 2] = a.precision;
}, emscripten_glGetShaderSource:(a, b, c, d) => {
  if (a = K.getShaderSource(O[a])) {
    b = 0 < b && d ? J(a, d, b) : 0, c && (z[c >> 2] = b);
  }
}, emscripten_glGetShaderiv:sf, emscripten_glGetString:a => {
  var b = ed[a];
  if (!b) {
    switch(a) {
      case 7939:
        b = pd(hf().join(" "));
        break;
      case 7936:
      case 7937:
      case 37445:
      case 37446:
        (b = K.getParameter(a)) || (T ||= 1280);
        b = b ? pd(b) : 0;
        break;
      case 7938:
        b = K.getParameter(7938);
        var c = `OpenGL ES 2.0 (${b})`;
        2 <= U.version && (c = `OpenGL ES 3.0 (${b})`);
        b = pd(c);
        break;
      case 35724:
        b = K.getParameter(35724);
        c = b.match(/^WebGL GLSL ES ([0-9]\.[0-9][0-9]?)(?:$| .*)/);
        null !== c && (3 == c[1].length && (c[1] += "0"), b = `OpenGL ES GLSL ES ${c[1]} (${b})`);
        b = pd(b);
        break;
      default:
        T ||= 1280;
    }
    ed[a] = b;
  }
  return b;
}, emscripten_glGetStringi:(a, b) => {
  if (2 > U.version) {
    return T ||= 1282, 0;
  }
  var c = fd[a];
  if (c) {
    return 0 > b || b >= c.length ? (T ||= 1281, 0) : c[b];
  }
  switch(a) {
    case 7939:
      return c = hf().map(pd), c = fd[a] = c, 0 > b || b >= c.length ? (T ||= 1281, 0) : c[b];
    default:
      return T ||= 1280, 0;
  }
}, emscripten_glGetSynciv:(a, b, c, d, e) => {
  0 > c ? T ||= 1281 : e ? (a = K.getSyncParameter(cd[a], b), null !== a && (z[e >> 2] = a, d && (z[d >> 2] = 1))) : T ||= 1281;
}, emscripten_glGetTexParameterfv:(a, b, c) => {
  c ? A[c >> 2] = K.getTexParameter(a, b) : T ||= 1281;
}, emscripten_glGetTexParameteriv:(a, b, c) => {
  c ? z[c >> 2] = K.getTexParameter(a, b) : T ||= 1281;
}, emscripten_glGetTransformFeedbackVarying:(a, b, c, d, e, f, g) => {
  a = N[a];
  if (a = K.getTransformFeedbackVarying(a, b)) {
    g && 0 < c ? (c = J(a.name, g, c), d && (z[d >> 2] = c)) : d && (z[d >> 2] = 0), e && (z[e >> 2] = a.size), f && (z[f >> 2] = a.type);
  }
}, emscripten_glGetUniformBlockIndex:(a, b) => K.getUniformBlockIndex(N[a], D(b)), emscripten_glGetUniformIndices:(a, b, c, d) => {
  if (d) {
    if (0 < b && (0 == c || 0 == d)) {
      T ||= 1281;
    } else {
      a = N[a];
      for (var e = [], f = 0; f < b; f++) {
        e.push(D(q[c + 4 * f >> 2]));
      }
      if (a = K.getUniformIndices(a, e)) {
        for (b = a.length, f = 0; f < b; f++) {
          z[d + 4 * f >> 2] = a[f];
        }
      }
    }
  } else {
    T ||= 1281;
  }
}, emscripten_glGetUniformLocation:vf, emscripten_glGetUniformfv:(a, b, c) => {
  wf(a, b, c, 2);
}, emscripten_glGetUniformiv:(a, b, c) => {
  wf(a, b, c, 0);
}, emscripten_glGetUniformuiv:(a, b, c) => wf(a, b, c, 0), emscripten_glGetVertexAttribIiv:yf, emscripten_glGetVertexAttribIuiv:yf, emscripten_glGetVertexAttribPointerv:(a, b, c) => {
  c ? (U.V[a].enabled && l("glGetVertexAttribPointer on client-side array: not supported, bad data returned"), z[c >> 2] = K.getVertexAttribOffset(a, b)) : T ||= 1281;
}, emscripten_glGetVertexAttribfv:(a, b, c) => {
  xf(a, b, c, 2);
}, emscripten_glGetVertexAttribiv:(a, b, c) => {
  xf(a, b, c, 5);
}, emscripten_glHint:(a, b) => K.hint(a, b), emscripten_glInvalidateFramebuffer:(a, b, c) => {
  for (var d = Oe[b], e = 0; e < b; e++) {
    d[e] = z[c + 4 * e >> 2];
  }
  K.invalidateFramebuffer(a, d);
}, emscripten_glInvalidateSubFramebuffer:(a, b, c, d, e, f, g) => {
  for (var m = Oe[b], p = 0; p < b; p++) {
    m[p] = z[c + 4 * p >> 2];
  }
  K.invalidateSubFramebuffer(a, m, d, e, f, g);
}, emscripten_glIsBuffer:a => (a = Vc[a]) ? K.isBuffer(a) : 0, emscripten_glIsEnabled:a => K.isEnabled(a), emscripten_glIsFramebuffer:a => (a = Xc[a]) ? K.isFramebuffer(a) : 0, emscripten_glIsProgram:a => (a = N[a]) ? K.isProgram(a) : 0, emscripten_glIsQuery:a => (a = P[a]) ? K.isQuery(a) : 0, emscripten_glIsQueryEXT:a => (a = P[a]) ? K.J.isQueryEXT(a) : 0, emscripten_glIsRenderbuffer:a => (a = Yc[a]) ? K.isRenderbuffer(a) : 0, emscripten_glIsSampler:a => (a = ad[a]) ? K.isSampler(a) : 0, emscripten_glIsShader:a => 
(a = O[a]) ? K.isShader(a) : 0, emscripten_glIsSync:a => K.isSync(cd[a]), emscripten_glIsTexture:a => (a = Zc[a]) ? K.isTexture(a) : 0, emscripten_glIsTransformFeedback:a => K.isTransformFeedback(bd[a]), emscripten_glIsVertexArray:zf, emscripten_glIsVertexArrayOES:zf, emscripten_glLineWidth:a => K.lineWidth(a), emscripten_glLinkProgram:Af, emscripten_glMapBufferRange:(a, b, c, d) => {
  if (0 != (d & 33)) {
    return l("glMapBufferRange access does not support MAP_READ or MAP_UNSYNCHRONIZED"), 0;
  }
  if (0 == (d & 2)) {
    return l("glMapBufferRange access must include MAP_WRITE"), 0;
  }
  if (0 == (d & 12)) {
    return l("glMapBufferRange access must include INVALIDATE_BUFFER or INVALIDATE_RANGE"), 0;
  }
  if (!We(a)) {
    return T ||= 1280, l("GL_INVALID_ENUM in glMapBufferRange"), 0;
  }
  var e = W(c);
  a = Ve(a);
  if (!e) {
    return 0;
  }
  let f;
  a = Wc[f = a] ?? (Wc[f] = {});
  a.offset = b;
  a.length = c;
  a.W = e;
  a.lb = d;
  return e;
}, emscripten_glPauseTransformFeedback:() => K.pauseTransformFeedback(), emscripten_glPixelStorei:Bf, emscripten_glPolygonModeWEBGL:(a, b) => {
  K.uc.polygonModeWEBGL(a, b);
}, emscripten_glPolygonOffset:(a, b) => K.polygonOffset(a, b), emscripten_glPolygonOffsetClampEXT:(a, b, c) => {
  K.Wb.polygonOffsetClampEXT(a, b, c);
}, emscripten_glProgramBinary:() => {
  T ||= 1280;
}, emscripten_glProgramParameteri:() => {
  T ||= 1280;
}, emscripten_glQueryCounterEXT:(a, b) => {
  K.J.queryCounterEXT(P[a], b);
}, emscripten_glReadBuffer:a => K.readBuffer(a), emscripten_glReadPixels:Ef, emscripten_glReleaseShaderCompiler:() => {
}, emscripten_glRenderbufferStorage:Ff, emscripten_glRenderbufferStorageMultisample:(a, b, c, d, e) => K.renderbufferStorageMultisample(a, b, c, d, e), emscripten_glResumeTransformFeedback:() => K.resumeTransformFeedback(), emscripten_glSampleCoverage:(a, b) => {
  K.sampleCoverage(a, !!b);
}, emscripten_glSamplerParameterf:(a, b, c) => {
  K.samplerParameterf(ad[a], b, c);
}, emscripten_glSamplerParameterfv:(a, b, c) => {
  K.samplerParameterf(ad[a], b, A[c >> 2]);
}, emscripten_glSamplerParameteri:(a, b, c) => {
  K.samplerParameteri(ad[a], b, c);
}, emscripten_glSamplerParameteriv:(a, b, c) => {
  K.samplerParameteri(ad[a], b, z[c >> 2]);
}, emscripten_glScissor:(a, b, c, d) => K.scissor(a, b, c, d), emscripten_glShaderBinary:() => {
  T ||= 1280;
}, emscripten_glShaderSource:Gf, emscripten_glStencilFunc:Hf, emscripten_glStencilFuncSeparate:(a, b, c, d) => K.stencilFuncSeparate(a, b, c, d), emscripten_glStencilMask:If, emscripten_glStencilMaskSeparate:(a, b) => K.stencilMaskSeparate(a, b), emscripten_glStencilOp:Jf, emscripten_glStencilOpSeparate:Kf, emscripten_glTexImage2D:Lf, emscripten_glTexImage3D:(a, b, c, d, e, f, g, m, p, r) => {
  if (K.S) {
    K.texImage3D(a, b, c, d, e, f, g, m, p, r);
  } else if (r) {
    var t = Cf(p);
    K.texImage3D(a, b, c, d, e, f, g, m, p, t, r >>> 31 - Math.clz32(t.BYTES_PER_ELEMENT));
  } else {
    K.texImage3D(a, b, c, d, e, f, g, m, p, null);
  }
}, emscripten_glTexParameterf:(a, b, c) => K.texParameterf(a, b, c), emscripten_glTexParameterfv:(a, b, c) => {
  K.texParameterf(a, b, A[c >> 2]);
}, emscripten_glTexParameteri:Mf, emscripten_glTexParameteriv:(a, b, c) => {
  K.texParameteri(a, b, z[c >> 2]);
}, emscripten_glTexStorage2D:(a, b, c, d, e) => K.texStorage2D(a, b, c, d, e), emscripten_glTexStorage3D:(a, b, c, d, e, f) => K.texStorage3D(a, b, c, d, e, f), emscripten_glTexSubImage2D:Nf, emscripten_glTexSubImage3D:(a, b, c, d, e, f, g, m, p, r, t) => {
  if (K.S) {
    K.texSubImage3D(a, b, c, d, e, f, g, m, p, r, t);
  } else if (t) {
    var v = Cf(r);
    K.texSubImage3D(a, b, c, d, e, f, g, m, p, r, v, t >>> 31 - Math.clz32(v.BYTES_PER_ELEMENT));
  } else {
    K.texSubImage3D(a, b, c, d, e, f, g, m, p, r, null);
  }
}, emscripten_glTransformFeedbackVaryings:(a, b, c, d) => {
  a = N[a];
  for (var e = [], f = 0; f < b; f++) {
    e.push(D(q[c + 4 * f >> 2]));
  }
  K.transformFeedbackVaryings(a, e, d);
}, emscripten_glUniform1f:(a, b) => {
  K.uniform1f(Z(a), b);
}, emscripten_glUniform1fv:(a, b, c) => {
  if (2 <= U.version) {
    b && K.uniform1fv(Z(a), A, c >> 2, b);
  } else {
    if (288 >= b) {
      for (var d = Of[b], e = 0; e < b; ++e) {
        d[e] = A[c + 4 * e >> 2];
      }
    } else {
      d = A.subarray(c >> 2, c + 4 * b >> 2);
    }
    K.uniform1fv(Z(a), d);
  }
}, emscripten_glUniform1i:Pf, emscripten_glUniform1iv:(a, b, c) => {
  if (2 <= U.version) {
    b && K.uniform1iv(Z(a), z, c >> 2, b);
  } else {
    if (288 >= b) {
      for (var d = Qf[b], e = 0; e < b; ++e) {
        d[e] = z[c + 4 * e >> 2];
      }
    } else {
      d = z.subarray(c >> 2, c + 4 * b >> 2);
    }
    K.uniform1iv(Z(a), d);
  }
}, emscripten_glUniform1ui:(a, b) => {
  K.uniform1ui(Z(a), b);
}, emscripten_glUniform1uiv:(a, b, c) => {
  b && K.uniform1uiv(Z(a), q, c >> 2, b);
}, emscripten_glUniform2f:(a, b, c) => {
  K.uniform2f(Z(a), b, c);
}, emscripten_glUniform2fv:Rf, emscripten_glUniform2i:(a, b, c) => {
  K.uniform2i(Z(a), b, c);
}, emscripten_glUniform2iv:(a, b, c) => {
  if (2 <= U.version) {
    b && K.uniform2iv(Z(a), z, c >> 2, 2 * b);
  } else {
    if (144 >= b) {
      b *= 2;
      for (var d = Qf[b], e = 0; e < b; e += 2) {
        d[e] = z[c + 4 * e >> 2], d[e + 1] = z[c + (4 * e + 4) >> 2];
      }
    } else {
      d = z.subarray(c >> 2, c + 8 * b >> 2);
    }
    K.uniform2iv(Z(a), d);
  }
}, emscripten_glUniform2ui:(a, b, c) => {
  K.uniform2ui(Z(a), b, c);
}, emscripten_glUniform2uiv:(a, b, c) => {
  b && K.uniform2uiv(Z(a), q, c >> 2, 2 * b);
}, emscripten_glUniform3f:(a, b, c, d) => {
  K.uniform3f(Z(a), b, c, d);
}, emscripten_glUniform3fv:(a, b, c) => {
  if (2 <= U.version) {
    b && K.uniform3fv(Z(a), A, c >> 2, 3 * b);
  } else {
    if (96 >= b) {
      b *= 3;
      for (var d = Of[b], e = 0; e < b; e += 3) {
        d[e] = A[c + 4 * e >> 2], d[e + 1] = A[c + (4 * e + 4) >> 2], d[e + 2] = A[c + (4 * e + 8) >> 2];
      }
    } else {
      d = A.subarray(c >> 2, c + 12 * b >> 2);
    }
    K.uniform3fv(Z(a), d);
  }
}, emscripten_glUniform3i:(a, b, c, d) => {
  K.uniform3i(Z(a), b, c, d);
}, emscripten_glUniform3iv:(a, b, c) => {
  if (2 <= U.version) {
    b && K.uniform3iv(Z(a), z, c >> 2, 3 * b);
  } else {
    if (96 >= b) {
      b *= 3;
      for (var d = Qf[b], e = 0; e < b; e += 3) {
        d[e] = z[c + 4 * e >> 2], d[e + 1] = z[c + (4 * e + 4) >> 2], d[e + 2] = z[c + (4 * e + 8) >> 2];
      }
    } else {
      d = z.subarray(c >> 2, c + 12 * b >> 2);
    }
    K.uniform3iv(Z(a), d);
  }
}, emscripten_glUniform3ui:(a, b, c, d) => {
  K.uniform3ui(Z(a), b, c, d);
}, emscripten_glUniform3uiv:(a, b, c) => {
  b && K.uniform3uiv(Z(a), q, c >> 2, 3 * b);
}, emscripten_glUniform4f:(a, b, c, d, e) => {
  K.uniform4f(Z(a), b, c, d, e);
}, emscripten_glUniform4fv:Sf, emscripten_glUniform4i:(a, b, c, d, e) => {
  K.uniform4i(Z(a), b, c, d, e);
}, emscripten_glUniform4iv:(a, b, c) => {
  if (2 <= U.version) {
    b && K.uniform4iv(Z(a), z, c >> 2, 4 * b);
  } else {
    if (72 >= b) {
      b *= 4;
      for (var d = Qf[b], e = 0; e < b; e += 4) {
        d[e] = z[c + 4 * e >> 2], d[e + 1] = z[c + (4 * e + 4) >> 2], d[e + 2] = z[c + (4 * e + 8) >> 2], d[e + 3] = z[c + (4 * e + 12) >> 2];
      }
    } else {
      d = z.subarray(c >> 2, c + 16 * b >> 2);
    }
    K.uniform4iv(Z(a), d);
  }
}, emscripten_glUniform4ui:(a, b, c, d, e) => {
  K.uniform4ui(Z(a), b, c, d, e);
}, emscripten_glUniform4uiv:(a, b, c) => {
  b && K.uniform4uiv(Z(a), q, c >> 2, 4 * b);
}, emscripten_glUniformBlockBinding:(a, b, c) => {
  a = N[a];
  K.uniformBlockBinding(a, b, c);
}, emscripten_glUniformMatrix2fv:(a, b, c, d) => {
  if (2 <= U.version) {
    b && K.uniformMatrix2fv(Z(a), !!c, A, d >> 2, 4 * b);
  } else {
    if (72 >= b) {
      b *= 4;
      for (var e = Of[b], f = 0; f < b; f += 4) {
        e[f] = A[d + 4 * f >> 2], e[f + 1] = A[d + (4 * f + 4) >> 2], e[f + 2] = A[d + (4 * f + 8) >> 2], e[f + 3] = A[d + (4 * f + 12) >> 2];
      }
    } else {
      e = A.subarray(d >> 2, d + 16 * b >> 2);
    }
    K.uniformMatrix2fv(Z(a), !!c, e);
  }
}, emscripten_glUniformMatrix2x3fv:(a, b, c, d) => {
  b && K.uniformMatrix2x3fv(Z(a), !!c, A, d >> 2, 6 * b);
}, emscripten_glUniformMatrix2x4fv:(a, b, c, d) => {
  b && K.uniformMatrix2x4fv(Z(a), !!c, A, d >> 2, 8 * b);
}, emscripten_glUniformMatrix3fv:(a, b, c, d) => {
  if (2 <= U.version) {
    b && K.uniformMatrix3fv(Z(a), !!c, A, d >> 2, 9 * b);
  } else {
    if (32 >= b) {
      b *= 9;
      for (var e = Of[b], f = 0; f < b; f += 9) {
        e[f] = A[d + 4 * f >> 2], e[f + 1] = A[d + (4 * f + 4) >> 2], e[f + 2] = A[d + (4 * f + 8) >> 2], e[f + 3] = A[d + (4 * f + 12) >> 2], e[f + 4] = A[d + (4 * f + 16) >> 2], e[f + 5] = A[d + (4 * f + 20) >> 2], e[f + 6] = A[d + (4 * f + 24) >> 2], e[f + 7] = A[d + (4 * f + 28) >> 2], e[f + 8] = A[d + (4 * f + 32) >> 2];
      }
    } else {
      e = A.subarray(d >> 2, d + 36 * b >> 2);
    }
    K.uniformMatrix3fv(Z(a), !!c, e);
  }
}, emscripten_glUniformMatrix3x2fv:(a, b, c, d) => {
  b && K.uniformMatrix3x2fv(Z(a), !!c, A, d >> 2, 6 * b);
}, emscripten_glUniformMatrix3x4fv:(a, b, c, d) => {
  b && K.uniformMatrix3x4fv(Z(a), !!c, A, d >> 2, 12 * b);
}, emscripten_glUniformMatrix4fv:(a, b, c, d) => {
  if (2 <= U.version) {
    b && K.uniformMatrix4fv(Z(a), !!c, A, d >> 2, 16 * b);
  } else {
    if (18 >= b) {
      var e = Of[16 * b], f = A;
      d >>= 2;
      b *= 16;
      for (var g = 0; g < b; g += 16) {
        var m = d + g;
        e[g] = f[m];
        e[g + 1] = f[m + 1];
        e[g + 2] = f[m + 2];
        e[g + 3] = f[m + 3];
        e[g + 4] = f[m + 4];
        e[g + 5] = f[m + 5];
        e[g + 6] = f[m + 6];
        e[g + 7] = f[m + 7];
        e[g + 8] = f[m + 8];
        e[g + 9] = f[m + 9];
        e[g + 10] = f[m + 10];
        e[g + 11] = f[m + 11];
        e[g + 12] = f[m + 12];
        e[g + 13] = f[m + 13];
        e[g + 14] = f[m + 14];
        e[g + 15] = f[m + 15];
      }
    } else {
      e = A.subarray(d >> 2, d + 64 * b >> 2);
    }
    K.uniformMatrix4fv(Z(a), !!c, e);
  }
}, emscripten_glUniformMatrix4x2fv:(a, b, c, d) => {
  b && K.uniformMatrix4x2fv(Z(a), !!c, A, d >> 2, 8 * b);
}, emscripten_glUniformMatrix4x3fv:(a, b, c, d) => {
  b && K.uniformMatrix4x3fv(Z(a), !!c, A, d >> 2, 12 * b);
}, emscripten_glUnmapBuffer:a => {
  if (!We(a)) {
    return T ||= 1280, l("GL_INVALID_ENUM in glUnmapBuffer"), 0;
  }
  var b = Wc[Ve(a)];
  if (!b || !b.W) {
    return T ||= 1282, l("buffer was never mapped in glUnmapBuffer"), 0;
  }
  b.lb & 16 || (2 <= U.version ? K.bufferSubData(a, b.offset, x, b.W, b.length) : K.bufferSubData(a, b.offset, x.subarray(b.W, b.W + b.length)));
  Og(b.W);
  b.W = 0;
  return 1;
}, emscripten_glUseProgram:Tf, emscripten_glValidateProgram:a => {
  K.validateProgram(N[a]);
}, emscripten_glVertexAttrib1f:(a, b) => K.vertexAttrib1f(a, b), emscripten_glVertexAttrib1fv:(a, b) => {
  K.vertexAttrib1f(a, A[b >> 2]);
}, emscripten_glVertexAttrib2f:(a, b, c) => K.vertexAttrib2f(a, b, c), emscripten_glVertexAttrib2fv:(a, b) => {
  K.vertexAttrib2f(a, A[b >> 2], A[b + 4 >> 2]);
}, emscripten_glVertexAttrib3f:(a, b, c, d) => K.vertexAttrib3f(a, b, c, d), emscripten_glVertexAttrib3fv:(a, b) => {
  K.vertexAttrib3f(a, A[b >> 2], A[b + 4 >> 2], A[b + 8 >> 2]);
}, emscripten_glVertexAttrib4f:(a, b, c, d, e) => K.vertexAttrib4f(a, b, c, d, e), emscripten_glVertexAttrib4fv:(a, b) => {
  K.vertexAttrib4f(a, A[b >> 2], A[b + 4 >> 2], A[b + 8 >> 2], A[b + 12 >> 2]);
}, emscripten_glVertexAttribDivisor:Uf, emscripten_glVertexAttribDivisorANGLE:Uf, emscripten_glVertexAttribDivisorARB:Uf, emscripten_glVertexAttribDivisorEXT:Uf, emscripten_glVertexAttribDivisorNV:Uf, emscripten_glVertexAttribI4i:(a, b, c, d, e) => K.vertexAttribI4i(a, b, c, d, e), emscripten_glVertexAttribI4iv:(a, b) => {
  K.vertexAttribI4i(a, z[b >> 2], z[b + 4 >> 2], z[b + 8 >> 2], z[b + 12 >> 2]);
}, emscripten_glVertexAttribI4ui:(a, b, c, d, e) => K.vertexAttribI4ui(a, b, c, d, e), emscripten_glVertexAttribI4uiv:(a, b) => {
  K.vertexAttribI4ui(a, q[b >> 2], q[b + 4 >> 2], q[b + 8 >> 2], q[b + 12 >> 2]);
}, emscripten_glVertexAttribIPointer:(a, b, c, d, e) => {
  var f = U.V[a];
  K.ia ? (f.ha = !1, K.vertexAttribIPointer(a, b, c, d, e)) : (f.size = b, f.type = c, f.Za = !1, f.stride = d, f.ma = e, f.ha = !0, f.ib = function(g, m, p, r, t, v) {
    this.vertexAttribIPointer(g, m, p, t, v);
  });
}, emscripten_glVertexAttribPointer:Vf, emscripten_glViewport:Wf, emscripten_glWaitSync:(a, b, c) => {
  c = Number(c);
  K.waitSync(cd[a], b, c);
}, emscripten_has_asyncify:() => 1, emscripten_request_fullscreen_strategy:(a, b, c) => Xf(a, {bb:z[c >> 2], Ha:z[c + 4 >> 2], Yb:z[c + 8 >> 2], Rb:b, xa:z[c + 12 >> 2], nb:z[c + 16 >> 2]}), emscripten_request_pointerlock:(a, b) => {
  a = Y(a);
  return a ? a.requestPointerLock ? Td() ? je(a) : b ? (Rd(je, 2, [a]), 1) : -2 : -1 : -4;
}, emscripten_resize_heap:a => {
  var b = x.length;
  a >>>= 0;
  k(a > b);
  if (2147483648 < a) {
    return l(`Cannot enlarge memory, requested ${a} bytes, but the limit is ${2147483648} bytes!`), !1;
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
      } catch (m) {
        l(`growMemory: Attempted to grow heap from ${d} bytes to ${f} bytes, but got error: ${m}`);
      }
      g = void 0;
    }
    if (g) {
      return !0;
    }
  }
  l(`Failed to grow the heap from ${b} bytes to ${e} bytes, not enough memory!`);
  return !1;
}, emscripten_sample_gamepad_data:Yf, emscripten_set_beforeunload_callback_on_thread:(a, b, c) => "undefined" == typeof onbeforeunload ? -1 : 1 !== c ? -5 : Zf(a, b), emscripten_set_blur_callback_on_thread:(a, b, c, d) => $f(a, b, c, d, 12, "blur"), emscripten_set_canvas_element_size:be, emscripten_set_element_css_size:(a, b, c) => {
  a = Y(a);
  if (!a) {
    return -4;
  }
  a.style.width = b + "px";
  a.style.height = c + "px";
  return 0;
}, emscripten_set_focus_callback_on_thread:(a, b, c, d) => $f(a, b, c, d, 13, "focus"), emscripten_set_fullscreenchange_callback_on_thread:(a, b, c, d) => {
  if (!X.fullscreenEnabled()) {
    return -1;
  }
  a = Y(a);
  if (!a) {
    return -4;
  }
  ag(a, b, c, d, "webkitfullscreenchange");
  return ag(a, b, c, d, "fullscreenchange");
}, emscripten_set_gamepadconnected_callback_on_thread:(a, b, c) => Yf() ? -1 : bg(a, b, c, 26, "gamepadconnected"), emscripten_set_gamepaddisconnected_callback_on_thread:(a, b, c) => Yf() ? -1 : bg(a, b, c, 27, "gamepaddisconnected"), emscripten_set_keydown_callback_on_thread:(a, b, c, d) => cg(a, b, c, d, 2, "keydown"), emscripten_set_keypress_callback_on_thread:(a, b, c, d) => cg(a, b, c, d, 1, "keypress"), emscripten_set_keyup_callback_on_thread:(a, b, c, d) => cg(a, b, c, d, 3, "keyup"), emscripten_set_main_loop:(a, 
b, c) => {
  Gd(() => dynCall_v(a), b, c);
}, emscripten_set_mousedown_callback_on_thread:(a, b, c, d) => eg(a, b, c, d, 5, "mousedown"), emscripten_set_mouseenter_callback_on_thread:(a, b, c, d) => eg(a, b, c, d, 33, "mouseenter"), emscripten_set_mouseleave_callback_on_thread:(a, b, c, d) => eg(a, b, c, d, 34, "mouseleave"), emscripten_set_mousemove_callback_on_thread:(a, b, c, d) => eg(a, b, c, d, 8, "mousemove"), emscripten_set_mouseup_callback_on_thread:(a, b, c, d) => eg(a, b, c, d, 6, "mouseup"), emscripten_set_pointerlockchange_callback_on_thread:(a, 
b, c, d) => document.body?.requestPointerLock ? (a = Y(a)) ? fg(a, b, c, d) : -4 : -1, emscripten_set_resize_callback_on_thread:(a, b, c, d) => gg(a, b, c, d), emscripten_set_touchcancel_callback_on_thread:(a, b, c, d) => hg(a, b, c, d, 25, "touchcancel"), emscripten_set_touchend_callback_on_thread:(a, b, c, d) => hg(a, b, c, d, 23, "touchend"), emscripten_set_touchmove_callback_on_thread:(a, b, c, d) => hg(a, b, c, d, 24, "touchmove"), emscripten_set_touchstart_callback_on_thread:(a, b, c, d) => 
hg(a, b, c, d, 22, "touchstart"), emscripten_set_visibilitychange_callback_on_thread:(a, b, c) => Xd[1] ? ig(a, b, c) : -4, emscripten_set_wheel_callback_on_thread:(a, b, c, d) => (a = Y(a)) ? "undefined" != typeof a.onwheel ? jg(a, b, c, d) : -1 : -4, emscripten_set_window_title:a => document.title = D(a), emscripten_sleep:kg, environ_get:(a, b) => {
  var c = 0, d = 0, e;
  for (e of og()) {
    var f = b + c;
    q[a + d >> 2] = f;
    c += J(e, f, Infinity) + 1;
    d += 4;
  }
  return 0;
}, environ_sizes_get:(a, b) => {
  var c = og();
  q[a >> 2] = c.length;
  a = 0;
  for (var d of c) {
    a += ub(d) + 1;
  }
  q[b >> 2] = a;
  return 0;
}, fd_close:function(a) {
  try {
    var b = Xb(a);
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
      var e = Xb(a);
      a = b;
      for (var f, g = b = 0; g < c; g++) {
        var m = q[a >> 2], p = q[a + 4 >> 2];
        a += 8;
        var r = E.read(e, w, m, p, f);
        if (0 > r) {
          var t = -1;
          break a;
        }
        b += r;
        if (r < p) {
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
    var e = Xb(a);
    E.D(e, b, c);
    Na[d >> 3] = BigInt(e.position);
    e.Oa && 0 === b && 0 === c && (e.Oa = null);
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
      var e = Xb(a);
      a = b;
      for (var f, g = b = 0; g < c; g++) {
        var m = q[a >> 2], p = q[a + 4 >> 2];
        a += 8;
        var r = E.write(e, w, m, p, f);
        if (0 > r) {
          var t = -1;
          break a;
        }
        b += r;
        if (r < p) {
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
}, glActiveTexture:le, glAttachShader:me, glBindAttribLocation:ne, glBindBuffer:oe, glBindFramebuffer:pe, glBindRenderbuffer:qe, glBindTexture:re, glBlendFuncSeparate:te, glBufferData:ue, glCheckFramebufferStatus:ve, glClear:we, glClearColor:xe, glColorMask:ye, glCompileShader:ze, glCreateProgram:Ae, glCreateShader:Be, glCullFace:Ce, glDeleteBuffers:De, glDeleteFramebuffers:Ee, glDeleteProgram:Fe, glDeleteRenderbuffers:Ge, glDeleteShader:He, glDeleteTextures:Ie, glDisable:Ke, glDisableVertexAttribArray:Le, 
glDrawArrays:Me, glEnable:Se, glEnableVertexAttribArray:Te, glFinish:Ue, glFramebufferRenderbuffer:Xe, glFramebufferTexture2D:Ye, glFrontFace:Ze, glGenBuffers:$e, glGenFramebuffers:af, glGenRenderbuffers:bf, glGenTextures:cf, glGenerateMipmap:ef, glGetError:kf, glGetIntegerv:mf, glGetProgramInfoLog:nf, glGetProgramiv:of, glGetShaderInfoLog:rf, glGetShaderiv:sf, glGetUniformLocation:vf, glLinkProgram:Af, glPixelStorei:Bf, glReadPixels:Ef, glRenderbufferStorage:Ff, glShaderSource:Gf, glStencilFunc:Hf, 
glStencilMask:If, glStencilOp:Jf, glStencilOpSeparate:Kf, glTexImage2D:Lf, glTexParameteri:Mf, glTexSubImage2D:Nf, glUniform1i:Pf, glUniform2fv:Rf, glUniform4fv:Sf, glUseProgram:Tf, glVertexAttribPointer:Vf, glViewport:Wf}, Wg;
function Xg(a = []) {
  k(0 == Ya, 'cannot call main when async dependencies remain! (listen on Module["onRuntimeInitialized"])');
  k("undefined" === typeof Wa || 0 == Wa.length, "cannot call main when preRun functions remain to be called");
  var b = Ug;
  a.unshift(ja);
  var c = a.length, d = Yd(4 * (c + 1)), e = d, f;
  for (f of a) {
    q[e >> 2] = Zd(f), e += 4;
  }
  q[e >> 2] = 0;
  try {
    var g = b(c, d);
    vc(g, !0);
  } catch (m) {
    sc(m);
  }
}
function Yg(a = ia) {
  function b() {
    k(!Wg);
    Wg = !0;
    h.calledRun = !0;
    if (!sa) {
      k(!Ha);
      Ha = !0;
      wa();
      if (!h.noFSInit && !E.Ba) {
        k(!E.Ba, "FS.init was previously called. If you want to initialize later with custom parameters, remove any earlier calls (note that one is automatically added to the generated code)");
        E.Ba = !0;
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
      E.ub = !1;
      wa();
      h.onRuntimeInitialized?.();
      Aa("onRuntimeInitialized");
      h.noInitialRun || Xg(a);
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
    Za = Yg;
  } else {
    Vg();
    ua();
    if (h.preRun) {
      for ("function" == typeof h.preRun && (h.preRun = [h.preRun]); h.preRun.length;) {
        Xa();
      }
    }
    Aa("preRun");
    Ua(Wa);
    0 < Ya ? Za = Yg : (h.setStatus ? (h.setStatus("Running..."), setTimeout(() => {
      setTimeout(() => h.setStatus(""), 1);
      b();
    }, 1)) : b(), wa());
  }
}
function uc() {
  var a = qa, b = l, c = !1;
  qa = l = () => {
    c = !0;
  };
  try {
    lc(0);
    for (var d of ["stdout", "stderr"]) {
      var e = fc("/dev/" + d);
      if (!e) {
        return;
      }
      xb[e.object.rdev]?.output?.length && (c = !0);
    }
  } catch (f) {
  }
  qa = a;
  l = b;
  c && Fa("stdio streams had content in them that was not flushed. you should set EXIT_RUNTIME to 1 (see the Emscripten FAQ), or make sure to emit a newline when you printf etc.");
}
var Oa;
(async function() {
  function a(d) {
    var e = Oa = d.exports;
    d = {};
    for (let [f, g] of Object.entries(e)) {
      "function" == typeof g ? (e = wg(g), d[f] = e) : d[f] = g;
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
    Ug = h._main = B("__main_argc_argv", 2);
    Og = B("free", 1);
    W = B("malloc", 1);
    lc = B("fflush", 1);
    h._web_on_file_ready = B("web_on_file_ready", 1);
    h._web_on_file_cancelled = B("web_on_file_cancelled", 0);
    kc = B("strerror", 1);
    va = d.emscripten_stack_get_end;
    Vg = d.emscripten_stack_init;
    $d = d._emscripten_stack_restore;
    Yd = d._emscripten_stack_alloc;
    rc = d.emscripten_stack_get_current;
    dynCall_v = C.v = B("dynCall_v", 1);
    C.ddd = B("dynCall_ddd", 3);
    C.dd = B("dynCall_dd", 2);
    C.d = B("dynCall_d", 1);
    C.viii = B("dynCall_viii", 4);
    C.iiiiii = B("dynCall_iiiiii", 6);
    C.viiiiii = B("dynCall_viiiiii", 7);
    ee = C.iiii = B("dynCall_iiii", 4);
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
    Mg = B("asyncify_start_unwind", 1);
    zg = B("asyncify_stop_unwind", 0);
    Lg = B("asyncify_start_rewind", 1);
    Ng = B("asyncify_stop_rewind", 0);
    Ja = d.memory;
    Ia();
    bb("wasm-instantiate");
    return Oa;
  }
  cb("wasm-instantiate");
  var b = h;
  rg();
  var c = {env:sg, wasi_snapshot_preview1:sg};
  if (h.instantiateWasm) {
    return new Promise((d, e) => {
      try {
        h.instantiateWasm(c, (f, g) => {
          d(a(f, g));
        });
      } catch (f) {
        l(`Module.instantiateWasm callback failed with error: ${f}`), e(f);
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
Yg();

