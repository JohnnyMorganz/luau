// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/BuiltinDefinitions.h"

LUAU_FASTFLAG(LuauUnknownAndNeverType)
LUAU_FASTFLAG(LuauCheckLenMT)

namespace Luau
{

static const std::string kBuiltinDefinitionLuaSrc = R"BUILTIN_SRC(

declare bit32: {
    band: (...number) -> number,
    bor: (...number) -> number,
    bxor: (...number) -> number,
    btest: (n: number, ...number) -> boolean,
    rrotate: (n: number, i: number) -> number,
    lrotate: (n: number, i: number) -> number,
    lshift: (n: number, i: number) -> number,
    arshift: (n: number, i: number) -> number,
    rshift: (n: number, i: number) -> number,
    bnot: (n: number) -> number,
    extract: (n: number, position: number, width: number?) -> number,
    replace: (n: number, r: number, position: number, width: number?) -> number,
    countlz: (n: number) -> number,
    countrz: (n: number) -> number,
}

declare math: {
    frexp: (number) -> (number, number),
    ldexp: (number, number) -> number,
    fmod: (number, number) -> number,
    modf: (number) -> (number, number),
    pow: (number, number) -> number,
    exp: (number) -> number,

    ceil: (number) -> number,
    floor: (number) -> number,
    abs: (number) -> number,
    sqrt: (number) -> number,

    log: (number, number?) -> number,
    log10: (number) -> number,

    rad: (number) -> number,
    deg: (number) -> number,

    sin: (number) -> number,
    cos: (number) -> number,
    tan: (number) -> number,
    sinh: (number) -> number,
    cosh: (number) -> number,
    tanh: (number) -> number,
    atan: (number) -> number,
    acos: (number) -> number,
    asin: (number) -> number,
    atan2: (number, number) -> number,

    min: (number, ...number) -> number,
    max: (number, ...number) -> number,

    pi: number,
    huge: number,

    randomseed: (number) -> (),
    random: (number?, number?) -> number,

    sign: (number) -> number,
    clamp: (number, number, number) -> number,
    noise: (number, number?, number?) -> number,
    round: (number) -> number,
}

type DateTypeArg = {
    year: number,
    month: number,
    day: number,
    hour: number?,
    min: number?,
    sec: number?,
    isdst: boolean?,
}

type DateTypeResult = {
    year: number,
    month: number,
    wday: number,
    yday: number,
    day: number,
    hour: number,
    min: number,
    sec: number,
    isdst: boolean,
}

declare os: {
    time: (time: DateTypeArg?) -> number,
    date: (format: string?, time: number?) -> DateTypeResult | string,
    difftime: (a: DateTypeResult | number, b: DateTypeResult | number) -> number,
    clock: () -> number,
}

declare function require(target: any): any

declare function getfenv(target: any): { [string]: any }

declare _G: any
declare _VERSION: string

declare function gcinfo(): number

declare function print<T...>(...: T...)

declare function type<T>(value: T): string
declare function typeof<T>(value: T): string

-- `assert` has a magic function attached that will give more detailed type information
declare function assert<T>(value: T, errorMessage: string?): T

declare function tostring<T>(value: T): string
declare function tonumber<T>(value: T, radix: number?): number?

declare function rawequal<T1, T2>(a: T1, b: T2): boolean
declare function rawget<K, V>(tab: {[K]: V}, k: K): V
declare function rawset<K, V>(tab: {[K]: V}, k: K, v: V): {[K]: V}

declare function setfenv<T..., R...>(target: number | (T...) -> R..., env: {[string]: any}): ((T...) -> R...)?

declare function ipairs<V>(tab: {V}): (({V}, number) -> (number, V), {V}, number)

declare function pcall<A..., R...>(f: (A...) -> R..., ...: A...): (boolean, R...)

-- FIXME: The actual type of `xpcall` is:
-- <E, A..., R1..., R2...>(f: (A...) -> R1..., err: (E) -> R2..., A...) -> (true, R1...) | (false, R2...)
-- Since we can't represent the return value, we use (boolean, R1...).
declare function xpcall<E, A..., R1..., R2...>(f: (A...) -> R1..., err: (E) -> R2..., ...: A...): (boolean, R1...)

-- `select` has a magic function attached to provide more detailed type information
declare function select<A...>(i: string | number, ...: A...): ...any

-- FIXME: This type is not entirely correct - `loadstring` returns a function or
-- (nil, string).
declare function loadstring<A...>(src: string, chunkname: string?): (((A...) -> any)?, string?)

declare function newproxy(mt: boolean?): any

declare coroutine: {
    create: <A..., R...>(f: (A...) -> R...) -> thread,
    resume: <A..., R...>(co: thread, A...) -> (boolean, R...),
    running: () -> thread,
    status: (co: thread) -> "dead" | "running" | "normal" | "suspended",
    -- FIXME: This technically returns a function, but we can't represent this yet.
    wrap: <A..., R...>(f: (A...) -> R...) -> any,
    yield: <A..., R...>(A...) -> R...,
    isyieldable: () -> boolean,
    close: (co: thread) -> (boolean, any)
}

declare table: {
    concat: <V>(tbl: {V}, sep: string?, from: number?, to: number?) -> string,
    insert: (<V>(tbl: {V}, value: V) -> ()) & (<V>(tbl: {V}, index: number, value: V) -> ()),
    maxn: <V>(tbl: {V}) -> number,
    remove: <V>(tbl: {V}, index: number?) -> V?,
    sort: <V>(tbl: {V}, f: ((V, V) -> boolean)?) -> (),
    create: <V>(size: number, value: V?) -> {V},
    find: <V>(tbl: {V}, value: V, startIndex: number?) -> number?,

    unpack: <V>(tbl: {V}, from: number?, to: number?) -> ...V,
    pack: <V>(...V) -> { n: number, [number]: V },

    getn: <V>(tbl: {V}) -> number,
    foreach: <K, V>(tbl: {[K]: V}, f: (key: K, value: V) -> ()) -> (),
    foreachi: <V>(tbl: {V}, f: (index: number, value: V) -> ()) -> (),

    move: <V>(tbl: {V}, from: number, to: number, startIndex: number, newTbl: {V}?) -> {V},
    clear: <K, V>(tbl: {[K]: V}) -> (),

    isfrozen: <K, V>(tbl: {[K]: V}) -> boolean,
}

declare debug: {
    info: (<R...>(co: thread, level: number, s: string) -> R...) & (<R...>(level: number, s: string) -> R...) & (<A..., R1..., R2...>(f: (A...) -> R1..., s: string) -> R2...),
    traceback: ((msg: string?, level: number?) -> string) & ((co: thread, message: string?, level: number?) -> string),
}

declare utf8: {
    char: (...number) -> string,
    charpattern: string,
    codes: (str: string) -> ((string, number) -> (number, number), string, number),
    -- FIXME
    codepoint: (str: string, startOffset: number?, endOffset: number?) -> (number, ...number),
    len: (str: string, startOffset: number?, endOffset: number?) -> (number?, number?),
    offset: (str: string, codepoint: number?, bytePosition: number?) -> number,
    nfdnormalize: (string) -> string,
    nfcnormalize: (string) -> string,
    graphemes: (string, number?, number?) -> (() -> (number, number)),
}

-- Cannot use `typeof` here because it will produce a polytype when we expect a monotype.
declare function unpack<V>(tab: {V}, i: number?, j: number?): ...V

)BUILTIN_SRC";

std::string getBuiltinDefinitionSource()
{

    std::string result = kBuiltinDefinitionLuaSrc;

    // TODO: move this into kBuiltinDefinitionLuaSrc
    if (FFlag::LuauCheckLenMT)
        result += "declare function rawlen<K, V>(obj: {[K]: V} | string): number\n";

    if (FFlag::LuauUnknownAndNeverType)
        result += "declare function error<T>(message: T, level: number?): never\n";
    else
        result += "declare function error<T>(message: T, level: number?)\n";

    return result;
}

} // namespace Luau
