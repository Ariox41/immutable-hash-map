import { useState } from 'react';
import { Highlight, themes } from 'prism-react-renderer';
import cppCode from '../include/imm/immutable_map.hpp?raw';

const sections = [
  { id: 'overview', label: 'Overview' },
  { id: 'design', label: 'Design' },
  { id: 'builder', label: 'Builder' },
  { id: 'hetero', label: 'Hetero Lookup' },
  { id: 'collect', label: 'Collect' },
  { id: 'api', label: 'API' },
  { id: 'source', label: 'Source' },
];

function CodeBlock({ code, language = 'cpp' }: { code: string; language?: string }) {
  return (
    <Highlight theme={themes.nightOwl} code={code.trim()} language={language}>
      {({ style, tokens, getLineProps, getTokenProps }) => (
        <pre
          style={{
            ...style,
            padding: '1.25rem',
            borderRadius: '0.75rem',
            overflow: 'auto',
            fontSize: '0.82rem',
            lineHeight: '1.55',
            margin: 0,
          }}
        >
          {tokens.map((line, i) => (
            <div key={i} {...getLineProps({ line })}>
              <span
                style={{
                  display: 'inline-block',
                  width: '2.5em',
                  textAlign: 'right',
                  paddingRight: '1em',
                  userSelect: 'none',
                  opacity: 0.4,
                }}
              >
                {i + 1}
              </span>
              {line.map((token, key) => (
                <span key={key} {...getTokenProps({ token })} />
              ))}
            </div>
          ))}
        </pre>
      )}
    </Highlight>
  );
}

function Section({ id, title, children }: { id: string; title: string; children: React.ReactNode }) {
  return (
    <section id={id} className="scroll-mt-20 mb-16">
      <h2 className="text-2xl font-bold text-white mb-6 pb-2 border-b border-slate-700">
        {title}
      </h2>
      {children}
    </section>
  );
}

function Param({ name, type, desc }: { name: string; type: string; desc: string }) {
  return (
    <div className="flex flex-col sm:flex-row sm:items-start gap-1 sm:gap-3 py-2 border-b border-slate-800">
      <code className="text-emerald-400 font-mono text-sm shrink-0">{name}</code>
      <code className="text-sky-400 font-mono text-sm shrink-0">{type}</code>
      <span className="text-slate-300 text-sm">{desc}</span>
    </div>
  );
}

export default function App() {
  const [activeSection, setActiveSection] = useState('overview');

  return (
    <div className="min-h-screen bg-slate-950 text-slate-200">
      {/* Header */}
      <header className="sticky top-0 z-50 bg-slate-900/90 backdrop-blur border-b border-slate-800">
        <div className="max-w-6xl mx-auto px-4 py-4 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className="w-8 h-8 rounded bg-gradient-to-br from-emerald-400 to-sky-500 flex items-center justify-center text-slate-900 font-bold text-sm">
              im
            </div>
            <h1 className="text-lg font-bold text-white">
              immutable_map<span className="text-slate-500 font-normal ml-1">C++17</span>
            </h1>
          </div>
          <nav className="hidden lg:flex gap-1">
            {sections.map((s) => (
              <a
                key={s.id}
                href={`#${s.id}`}
                onClick={() => setActiveSection(s.id)}
                className={`px-3 py-1.5 rounded text-sm transition-colors ${
                  activeSection === s.id
                    ? 'bg-slate-700 text-white'
                    : 'text-slate-400 hover:text-white hover:bg-slate-800'
                }`}
              >
                {s.label}
              </a>
            ))}
          </nav>
        </div>
      </header>

      <main className="max-w-6xl mx-auto px-4 py-12">
        {/* Overview */}
        <Section id="overview" title="Overview">
          <p className="text-slate-300 leading-relaxed mb-4">
            <code className="text-emerald-400 bg-slate-800 px-1.5 py-0.5 rounded text-sm">immutable_map</code> — шаблонный
            хэш-контейнер на C++17, в котором <strong className="text-white">структура данных неизменяема</strong> после
            инициализации, но значения доступны для модификации. Ключи иммутабельны; значения — нет.
          </p>
          <div className="grid sm:grid-cols-2 lg:grid-cols-3 gap-4 mt-6">
            {[
              { icon: '🔒', title: 'Immutable structure', desc: 'Keys and topology fixed after build()' },
              { icon: '📦', title: 'Single allocation', desc: 'Pairs and hash index in one block' },
              { icon: '🏗️', title: 'Builder pattern', desc: 'reserve → emplace → build' },
              { icon: '🧩', title: 'allocator_traits', desc: 'All ops via traits; EBO via inheritance' },
              { icon: '🚫', title: 'No move/copy for Value', desc: 'Values constructed in-place' },
              { icon: '📐', title: 'Copy via CRTP base', desc: 'Copy ops only when Value is copyable' },
              { icon: '🔍', title: 'Heterogeneous lookup', desc: 'find(string_view) on map<string, ...>' },
              { icon: '🔗', title: 'std::pair<const K, V>', desc: 'Standard pair type, real pair& iterator' },
              { icon: '🛡️', title: 'Exception safety', desc: 'Key rollback if Value ctor throws' },
            ].map((item) => (
              <div key={item.title} className="bg-slate-900 border border-slate-800 rounded-lg p-4">
                <div className="text-2xl mb-2">{item.icon}</div>
                <h3 className="text-white font-semibold text-sm mb-1">{item.title}</h3>
                <p className="text-slate-400 text-sm">{item.desc}</p>
              </div>
            ))}
          </div>
        </Section>

        {/* Design */}
        <Section id="design" title="Memory Layout & Architecture">
          <p className="text-slate-300 leading-relaxed mb-4">
            Контейнер выделяет <strong className="text-white">один блок памяти</strong> через аллокатор.
            Пары <code className="text-emerald-400">std::pair&lt;const Key, Value&gt;</code> хранятся рядом
            (ключ + значение в одной кэш-линии), индекс — отдельно.
          </p>

          <div className="bg-slate-900 border border-slate-800 rounded-lg p-6 mb-6 font-mono text-sm">
            <div className="text-slate-400 mb-3">Single allocation layout:</div>
            <div className="flex flex-wrap gap-0">
              <div className="border border-emerald-600 bg-emerald-900/30 px-4 py-2 rounded-l">
                <span className="text-emerald-400">pair&lt;const K,V&gt;[N]</span>
              </div>
              <div className="border border-yellow-600 bg-yellow-900/20 px-3 py-2 border-l-0">
                <span className="text-yellow-500 text-xs">pad</span>
              </div>
              <div className="border border-purple-600 bg-purple-900/30 px-4 py-2 rounded-r border-l-0">
                <span className="text-purple-400">uint32_t[M]</span>
              </div>
            </div>
            <div className="text-slate-500 mt-3 text-xs">
              M = next_power_of_2(2·N) — load factor ≤ 0.5, open addressing, linear probing.
              <br/>
              Index slots store uint32_t offsets into the pairs array, or UINT32_MAX for empty.
            </div>
          </div>

          <div className="grid md:grid-cols-2 gap-6">
            <div className="bg-slate-900 border border-slate-800 rounded-lg p-5">
              <h3 className="text-white font-semibold mb-3">Class hierarchy</h3>
              <div className="font-mono text-sm space-y-2 text-slate-300">
                <div>
                  <span className="text-sky-400">map_storage</span>
                  <span className="text-slate-500"> : private Alloc</span>
                </div>
                <div className="pl-4 text-slate-500">↑ private</div>
                <div>
                  <span className="text-emerald-400">immutable_map</span>
                  <span className="text-slate-500"> — lookup, iterators</span>
                </div>
                <div className="pl-4 text-slate-500">↑ private</div>
                <div>
                  <span className="text-purple-400">copy_support_base</span>
                  <span className="text-slate-500"> — conditional copy (CRTP)</span>
                </div>
              </div>
              <div className="mt-4 pt-3 border-t border-slate-800">
                <div className="font-mono text-sm space-y-2 text-slate-300">
                  <div>
                    <span className="text-sky-400">map_storage</span>
                    <span className="text-slate-500"> — (contained)</span>
                  </div>
                  <div className="pl-4 text-slate-500">↓ contains</div>
                  <div>
                    <span className="text-amber-400">immutable_map_builder</span>
                    <span className="text-slate-500"> — +capacity_, emplace, build</span>
                  </div>
                </div>
              </div>
            </div>

            <div className="bg-slate-900 border border-slate-800 rounded-lg p-5">
              <h3 className="text-white font-semibold mb-3">Allocator: EBO + traits + alignment</h3>
              <ul className="text-slate-300 text-sm space-y-2">
                <li className="flex gap-2">
                  <span className="text-emerald-400 shrink-0">•</span>
                  <span><code className="text-sky-300">map_storage</code> inherits from <code className="text-sky-300">Alloc</code> privately — EBO for stateless allocators</span>
                </li>
                <li className="flex gap-2">
                  <span className="text-emerald-400 shrink-0">•</span>
                  <span>All ops via <code className="text-sky-300">std::allocator_traits</code>, construct/destroy included</span>
                </li>
                <li className="flex gap-2">
                  <span className="text-emerald-400 shrink-0">•</span>
                  <span>Rebind to <code className="text-sky-300">align_block</code> (alignas max(pair_type, uint32_t)) — correct alignment guaranteed</span>
                </li>
                <li className="flex gap-2">
                  <span className="text-emerald-400 shrink-0">•</span>
                  <span><code className="text-sky-300">std::launder</code> for all pointer access after reinterpret_cast</span>
                </li>
                <li className="flex gap-2">
                  <span className="text-emerald-400 shrink-0">•</span>
                  <span>No <code className="text-sky-300">capacity_</code> in storage — only in builder</span>
                </li>
              </ul>
            </div>
          </div>

          <div className="mt-6 bg-slate-900 border border-slate-800 rounded-lg p-5">
            <h3 className="text-white font-semibold mb-3">Exception safety</h3>
            <p className="text-slate-300 text-sm mb-3">
              Используем <code className="text-sky-300">std::pair</code> piecewise constructor.
              Если конструктор Value бросает — pair не создан, ничего не нужно уничтожать.
              <code className="text-sky-300">size_</code> не инкрементируется.
            </p>
            <CodeBlock
              language="cpp"
              code={`// In try_emplace(piecewise, key_tuple, value_tuple):
using pair_alloc    = typename storage_t::pair_alloc;
using pair_alloc_tr = typename storage_t::pair_alloc_tr;
pair_alloc pa(get_alloc());

pair_alloc_tr::construct(pa, pair_at(size_),
                         std::piecewise_construct,
                         std::move(key_tuple),
                         std::move(value_tuple));
++size_;

// If Value constructor throws inside piecewise_construct,
// pair is not created — no cleanup needed. Simple!`}
            />
          </div>

          <div className="mt-6 bg-slate-900 border border-slate-800 rounded-lg p-5">
            <h3 className="text-white font-semibold mb-3">Alignment &amp; std::launder</h3>
            <p className="text-slate-300 text-sm mb-3">
              Ключевая проблема: <code className="text-sky-300">allocator&lt;std::byte&gt;::allocate</code> гарантирует
              выравнивание только для <code className="text-sky-300">std::byte</code> (alignof = 1).
              Нам нужно <code className="text-sky-300">alignof(pair_type)</code> и <code className="text-sky-300">alignof(uint32_t)</code>.
            </p>
            <CodeBlock
              language="cpp"
              code={`// Решение: align_block с нужным выравниванием
static constexpr std::size_t required_alignment =
    alignof(pair_type) > alignof(uint32_t)
        ? alignof(pair_type) : alignof(uint32_t);

struct alignas(required_alignment) align_block {
    std::byte data[1];
};

// Ребаундим аллокатор к align_block
using storage_alloc =
    typename alloc_traits::template rebind_alloc<align_block>;

// allocate/deallocate через storage_alloc_tr — выравнивание гарантировано`}
            />
            <p className="text-slate-300 text-sm mt-3 mb-2">
              <strong className="text-white">std::launder</strong> используется для:
            </p>
            <ul className="text-slate-300 text-sm space-y-1 ml-4">
              <li>• <code className="text-sky-300">pair_at(i)</code> — доступ к i-й паре через reinterpret_cast</li>
              <li>• <code className="text-sky-300">index()</code> — доступ к массиву uint32_t через reinterpret_cast</li>
            </ul>
            <p className="text-slate-300 text-sm mt-2">
              Pair создаётся целиком через piecewise constructor, поэтому <code className="text-sky-300">std::launder</code>{' '}
              корректен для pair access.
            </p>
          </div>
        </Section>

        {/* Builder */}
        <Section id="builder" title="Builder Pattern">
          <p className="text-slate-300 leading-relaxed mb-4">
            Вставка через <code className="text-emerald-400">immutable_map_builder</code>.
            Оба метода <code className="text-sky-400">try_emplace</code> и <code className="text-sky-400">emplace</code>
            имеют два варианта: с piecewise для обоих (ключ и значение) и с piecewise только для значения.
          </p>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-4 mb-4">
            <h4 className="text-white font-semibold mb-2 text-sm">API hierarchy</h4>
            <ul className="text-slate-300 text-sm space-y-1">
              <li><code className="text-emerald-400">try_emplace(piecewise, key_tuple, value_tuple)</code> — piecewise for both</li>
              <li><code className="text-emerald-400">try_emplace(key, args...)</code> — key as-is, piecewise for value</li>
              <li className="mt-2"><code className="text-sky-400">emplace(piecewise, key_tuple, value_tuple)</code> — piecewise for both</li>
              <li><code className="text-sky-400">emplace(key, args...)</code> — alias for try_emplace</li>
              <li className="mt-2"><code className="text-sky-400">insert(first, last)</code> — range of pairs {'{'}key, value{'}'}</li>
              <li><code className="text-sky-400">insert_piecewise(first, last)</code> — range of {'{'}key, tuple{'}'}, piecewise for value only</li>
            </ul>
          </div>
          <CodeBlock
            language="cpp"
            code={`imm::immutable_map_builder<std::string, int> builder;
builder.reserve(100);                    // allocate once

// try_emplace: piecewise for both key and value
builder.try_emplace(std::piecewise_construct,
                    std::forward_as_tuple("alpha"),
                    std::forward_as_tuple(1));

// try_emplace: key as-is, piecewise for value
builder.try_emplace("beta", 2);

// emplace: piecewise for both key and value
builder.emplace(std::piecewise_construct,
                std::forward_as_tuple("gamma"),
                std::forward_as_tuple(3));

// emplace: key as-is (alias for try_emplace)
builder.emplace("delta", 4);

// Range insertion: insert(first, last)
std::vector<std::pair<std::string, int>> pairs = {
    {"epsilon", 5}, {"zeta", 6}
};
builder.insert(pairs.begin(), pairs.end());

// Range with tuple: insert_piecewise(first, last)
// Piecewise only for value, key as-is
struct Point { int x, y; Point(int a, int b) : x(a), y(b) {} };
std::vector<std::pair<std::string, std::tuple<int, int>>> data = {
    {"origin", {0, 0}}
};
builder.insert_piecewise(data.begin(), data.end());

auto map = builder.build();              // build index, transfer ownership
assert(map.size() == 7);

// Values are mutable:
map.at("beta") = 42;

// Keys are immutable (const Key in pair):
// map.begin()->first = "x";  // ERROR: const std::string&`}
          />
          <p className="text-slate-300 leading-relaxed mt-4 mb-4">
            Для значений без конструктора копирования/перемещения:
          </p>
          <CodeBlock
            language="cpp"
            code={`struct NonMovable {
    std::unique_ptr<int> data;
    NonMovable(int x) : data(std::make_unique<int>(x)) {}
    NonMovable(const NonMovable&) = delete;
    NonMovable(NonMovable&&) = delete;
};

imm::immutable_map_builder<std::string, NonMovable> b;
b.reserve(2);
b.emplace("a", 10);    // NonMovable(10) in-place
b.emplace("b", 20);
auto m = b.build();    // OK — values never moved!
assert(*m.at("a").data == 10);`}
          />
        </Section>

        {/* Heterogeneous lookup */}
        <Section id="hetero" title="Heterogeneous Lookup">
          <p className="text-slate-300 leading-relaxed mb-4">
            <code className="text-emerald-400">find()</code>, <code className="text-emerald-400">at()</code>,{' '}
            <code className="text-emerald-400">contains()</code> — шаблонные по типу ключа.
            Пользователь сам гарантирует, что Hash и Eq работают с совместимым типом.
            Никаких тегов <code className="text-slate-500">is_transparent</code>.
          </p>
          <CodeBlock
            language="cpp"
            code={`// Key = std::string, but hash/eq work with string_view
struct string_hash {
    std::size_t operator()(std::string_view sv) const {
        return xxh3(sv.data(), sv.size());
    }
};

struct string_eq {
    bool operator()(std::string_view a, std::string_view b) const {
        return a == b;
    }
};

imm::immutable_map<std::string, int, string_hash, string_eq> map;
// ... build map ...

// Lookup by string_view — no temporary string!
std::string_view key = "hello";
auto val = map.find(key);          // Returns iterator
if (val != map.end()) {            // Check if found
    int x = val->second;           // OK
}
int y = map.at(key);               // OK, throws if not found
bool has = map.contains(key);      // OK

// Also works with const char*:
map.find("literal");               // OK — implicit → string_view`}
          />
          <div className="mt-4 bg-slate-900 border border-slate-800 rounded-lg p-5">
            <h3 className="text-white font-semibold mb-2 text-sm">noexcept</h3>
            <p className="text-slate-300 text-sm">
              <code className="text-sky-300">find()</code> и <code className="text-sky-300">contains()</code> помечены{' '}
              <code className="text-sky-300">noexcept</code> условно — если Hash и KeyEq noexcept.
              <code className="text-sky-300">at()</code> не noexcept (может бросить <code className="text-sky-300">out_of_range</code>).
            </p>
          </div>
        </Section>

        {/* Collect */}
        <Section id="collect" title="Collect from Range">
          <p className="text-slate-300 leading-relaxed mb-4">
            Функции <code className="text-emerald-400">collect</code> и{' '}
            <code className="text-emerald-400">collect_piecewise</code> — convenience wrappers
            над <code className="text-sky-400">insert</code> и <code className="text-sky-400">insert_piecewise</code>.
            <code className="text-sky-400">collect</code> имеет перегрузку с автоматическим выводом типов.
          </p>
          <CodeBlock
            language="cpp"
            code={`// collect: range of {key, value} pairs
// Internally: builder.reserve(n); builder.insert(first, last); builder.build();
std::vector<std::pair<std::string, int>> pairs = {
    {"one", 1}, {"two", 2}, {"three", 3}
};

// With explicit types
auto m1 = imm::collect<std::string, int>(pairs.begin(), pairs.end());
auto m1b = imm::collect<std::string, int>(pairs);

// With automatic type deduction (from pair<K,V>)
auto m1c = imm::collect(pairs);  // Deduces map<string, int>

// collect_piecewise: range of {key, tuple<args...>}
// Internally: builder.reserve(n); builder.insert_piecewise(first, last); builder.build();
// No std::apply needed — tuple passed directly to piecewise_construct
struct Point { int x, y; Point(int a, int b) : x(a), y(b) {} };

std::vector<std::pair<std::string, std::tuple<int, int>>> data = {
    {"origin", {0, 0}},
    {"unit",   {1, 1}},
};
// Point constructed as Point(0,0), Point(1,1)
auto m2 = imm::collect_piecewise<std::string, Point>(data);
assert(m2.at("origin").x == 0);`}
          />
          <p className="text-slate-300 leading-relaxed mt-4">
            Это соответствует стандартным контейнерам: <code className="text-sky-400">std::map::insert(first, last)</code>{' '}
            вставляет диапазон, а <code className="text-sky-400">collect</code> — это <code className="text-sky-400">insert</code> +{' '}
            <code className="text-sky-400">build</code> для immutable контейнера.
          </p>
        </Section>

        {/* API */}
        <Section id="api" title="API Reference">
          <h3 className="text-lg font-semibold text-white mb-3">Template Parameters</h3>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-4 mb-6">
            <Param name="Key" type="typename" desc="Copyable, movable. Stored as const in pair." />
            <Param name="Value" type="typename" desc="No move/copy required; constructed in-place" />
            <Param name="Hash" type="= std::hash<Key>" desc="Hash functor. Must work with CompatibleKey." />
            <Param name="KeyEq" type="= std::equal_to<Key>" desc="Equality functor. Must work with CompatibleKey." />
            <Param name="Alloc" type="= std::allocator<std::byte>" desc="Allocator. EBO via private inheritance." />
          </div>

          <h3 className="text-lg font-semibold text-white mb-3">immutable_map</h3>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-4 mb-6">
            <Param name="size()" type="→ size_type" desc="noexcept" />
            <Param name="empty()" type="→ bool" desc="noexcept" />
            <Param name="get_allocator()" type="→ allocator_type" desc="noexcept" />
            <Param name="find(key)" type="→ iterator" desc="template<CompatibleKey>. Returns end() if not found. Conditional noexcept." />
            <Param name="at(key)" type="→ Value&" desc="template<CompatibleKey>. Throws out_of_range." />
            <Param name="contains(key)" type="→ bool" desc="template<CompatibleKey>. Conditional noexcept." />
            <Param name="begin()/end()" type="→ iterator" desc="Forward iterator, returns pair&" />
          </div>

          <h3 className="text-lg font-semibold text-white mb-3">immutable_map_builder</h3>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-4 mb-6">
            <Param name="reserve(n)" type="void" desc="Throws: bad_alloc, logic_error" />
            <Param name="try_emplace(piecewise, kt, vt)" type="void" desc="Piecewise for both key and value" />
            <Param name="try_emplace(key, args...)" type="void" desc="Key as-is, piecewise for value" />
            <Param name="emplace(piecewise, kt, vt)" type="void" desc="Piecewise for both key and value" />
            <Param name="emplace(key, args...)" type="void" desc="Alias for try_emplace (standard compat)" />
            <Param name="insert(first, last)" type="void" desc="Range of pairs {key, value}" />
            <Param name="insert_piecewise(first, last)" type="void" desc="Range of {key, tuple}, piecewise for value only" />
            <Param name="build()" type="→ immutable_map" desc="Throws: logic_error, Hash exceptions." />
            <Param name="size()" type="→ size_type" desc="noexcept" />
          </div>

          <h3 className="text-lg font-semibold text-white mb-3">Free Functions</h3>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-4">
            <Param name="collect<K,V>(first, last)" type="→ immutable_map" desc="From {key, value} range with explicit types" />
            <Param name="collect<K,V>(range)" type="→ immutable_map" desc="From range object with explicit types" />
            <Param name="collect(range)" type="→ immutable_map" desc="Automatic type deduction from pair<K,V>" />
            <Param name="collect_piecewise<K,V>(first, last)" type="→ immutable_map" desc="From {key, tuple<args...>} range" />
            <Param name="collect_piecewise<K,V>(range)" type="→ immutable_map" desc="From range object" />
          </div>
        </Section>

        {/* Source */}
        <Section id="source" title="Full Source Code">
          <p className="text-slate-300 mb-4">
            Complete implementation in a single header file. No external dependencies beyond the C++17 standard library.
          </p>
          <div className="relative">
            <button
              onClick={() => navigator.clipboard.writeText(cppCode)}
              className="absolute top-3 right-3 z-10 bg-slate-700 hover:bg-slate-600 text-white text-xs px-3 py-1.5 rounded transition-colors"
            >
              Copy
            </button>
            <CodeBlock code={cppCode} language="cpp" />
          </div>
        </Section>
      </main>

      <footer className="border-t border-slate-800 py-8 text-center text-slate-500 text-sm">
        <p>immutable_map — C++17 header-only immutable hash map</p>
        <p className="mt-1">allocator_traits · EBO · Exception safety · Heterogeneous lookup · CRTP copy</p>
      </footer>
    </div>
  );
}
