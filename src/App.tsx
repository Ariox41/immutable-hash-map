import { useState } from 'react';
import { Highlight, themes } from 'prism-react-renderer';
import cppCode from './immutable_map.hpp?raw';

const sections = [
  { id: 'overview', label: 'Overview' },
  { id: 'design', label: 'Design' },
  { id: 'builder', label: 'Builder' },
  { id: 'hetero', label: 'Hetero Lookup' },
  { id: 'collect', label: 'Collect' },
  { id: 'api', label: 'API' },
  { id: 'examples', label: 'Examples' },
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
              { icon: '📦', title: 'Single allocation', desc: 'Pairs and hash index in one memory block' },
              { icon: '🏗️', title: 'Builder pattern', desc: 'reserve → emplace → build' },
              { icon: '🧩', title: 'Allocator-aware', desc: 'Any standard-compatible allocator' },
              { icon: '🚫', title: 'No move/copy for Value', desc: 'Values constructed in-place, never moved' },
              { icon: '📐', title: 'Copy via CRTP base', desc: 'Copy ops only when Value is copyable' },
              { icon: '🔍', title: 'Heterogeneous lookup', desc: 'find(string_view) on map<string, ...>' },
              { icon: '🔗', title: 'std::pair<const K, V>', desc: 'Standard pair type, compatible iterators' },
              { icon: '📊', title: 'Forward iterator', desc: 'Returns pair&, not proxy' },
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
                  <span className="text-slate-500"> — allocation, layout, lifetime</span>
                </div>
                <div className="pl-4 text-slate-500">↑ private</div>
                <div>
                  <span className="text-emerald-400">immutable_map</span>
                  <span className="text-slate-500"> — lookup, iterators</span>
                </div>
                <div className="pl-4 text-slate-500">↑ private</div>
                <div>
                  <span className="text-purple-400">copy_support_base</span>
                  <span className="text-slate-500"> — conditional copy ops (CRTP)</span>
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
              <h3 className="text-white font-semibold mb-3">Why pairs together?</h3>
              <ul className="text-slate-300 text-sm space-y-2">
                <li className="flex gap-2">
                  <span className="text-emerald-400 shrink-0">•</span>
                  <span>Iterator returns real <code className="text-sky-300">pair&amp;</code> — compatible with std algorithms</span>
                </li>
                <li className="flex gap-2">
                  <span className="text-emerald-400 shrink-0">•</span>
                  <span>Structured bindings work natively</span>
                </li>
                <li className="flex gap-2">
                  <span className="text-emerald-400 shrink-0">•</span>
                  <span>Key + value in same cache line during lookup</span>
                </li>
                <li className="flex gap-2">
                  <span className="text-emerald-400 shrink-0">•</span>
                  <span><code className="text-sky-300">const Key</code> makes pair non-movable — exactly what we need</span>
                </li>
              </ul>
            </div>
          </div>

          <div className="mt-6 bg-slate-900 border border-slate-800 rounded-lg p-5">
            <h3 className="text-white font-semibold mb-3">Copy via CRTP inheritance</h3>
            <p className="text-slate-300 text-sm mb-3">
              Copy constructor и copy assignment генерируются условно через базовый класс{' '}
              <code className="text-purple-400">copy_support_base</code>. Если Value не копируем — базовый класс
              пустой, copy ops implicitly deleted. Работает на GCC8+ без проблем с SFINAE на non-template operator=.
            </p>
            <CodeBlock
              language="cpp"
              code={`template <typename Derived, typename Value,
          bool = std::is_copy_constructible_v<Value>>
struct copy_support_base {
    // Value NOT copyable: copy ops implicitly deleted
};

template <typename Derived, typename Value>
struct copy_support_base<Derived, Value, true> {
    void copy_from(const Derived& other) { /* ... */ }
    Derived& copy_assign(Derived& self, const Derived& other) { /* ... */ }
};

class immutable_map
    : private map_storage<...>,
      private copy_support_base<immutable_map, Value> {
    // Copy ctor calls copy_base_t::copy_from(other)
    // Copy assign calls copy_base_t::copy_assign(*this, other)
};`}
            />
          </div>
        </Section>

        {/* Builder */}
        <Section id="builder" title="Builder Pattern">
          <p className="text-slate-300 leading-relaxed mb-4">
            Вставка элементов через <code className="text-emerald-400">immutable_map_builder</code>.
            Память выделяется один раз в <code className="text-sky-400">reserve()</code>,
            объекты конструируются in-place, индекс строится в <code className="text-sky-400">build()</code>.
          </p>
          <CodeBlock
            language="cpp"
            code={`imm::immutable_map_builder<std::string, int> builder;
builder.reserve(100);                    // allocate once
builder.emplace("alpha", 1);             // in-place construction
builder.emplace("beta", 2);
builder.emplace("gamma", 3);
auto map = builder.build();              // build index, transfer ownership

// After build(), builder is empty. map owns the data.
assert(map.size() == 3);
assert(map.at("beta") == 2);

// Values are mutable:
map.at("beta") = 42;
assert(map.at("beta") == 42);

// Keys are immutable (const Key in pair):
// map.begin()->first = "delta";  // ERROR: const std::string&`}
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
b.emplace("a", 10);    // constructs NonMovable(10) in-place
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
            Пользователь сам гарантирует, что Hash и Eq работают с совместимым типом ключа.
            Никаких тегов <code className="text-slate-500">is_transparent</code> не нужно.
          </p>
          <CodeBlock
            language="cpp"
            code={`// Key = std::string, but hash/eq work with string_view
struct string_hash {
    std::size_t operator()(std::string_view sv) const {
        // xxh3 or whatever — same result for string and string_view
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

// Lookup by string_view — no temporary string allocation!
std::string_view key = "hello";
auto* val = map.find(key);         // OK
int x = map.at(key);               // OK
bool has = map.contains(key);      // OK

// Also works with const char*:
map.find("literal");               // OK — implicit conversion to string_view`}
          />
          <div className="mt-4 bg-slate-900 border border-slate-800 rounded-lg p-5">
            <h3 className="text-white font-semibold mb-2 text-sm">Why this works</h3>
            <ul className="text-slate-300 text-sm space-y-2">
              <li className="flex gap-2">
                <span className="text-emerald-400 shrink-0">•</span>
                <span>
                  <code className="text-sky-300">hash(string_view)</code> и <code className="text-sky-300">hash(string)</code> дают
                  одинаковый результат — пользователь гарантирует это реализацией
                </span>
              </li>
              <li className="flex gap-2">
                <span className="text-emerald-400 shrink-0">•</span>
                <span>
                  <code className="text-sky-300">std::string</code> неявно конвертируется в <code className="text-sky-300">string_view</code>,
                  поэтому <code className="text-sky-300">eq(stored_key, query_key)</code> работает
                </span>
              </li>
              <li className="flex gap-2">
                <span className="text-emerald-400 shrink-0">•</span>
                <span>
                  Индексный слот находится по хэшу совместимого ключа — он совпадает с хэшем хранимого
                </span>
              </li>
            </ul>
          </div>
        </Section>

        {/* Collect */}
        <Section id="collect" title="Collect from Range">
          <p className="text-slate-300 leading-relaxed mb-4">
            Функция <code className="text-emerald-400">collect</code> строит карту из диапазона.
            Поддерживаются forward_iterator и выше. Автоматически определяет, является ли .second кортежем.
          </p>
          <CodeBlock
            language="cpp"
            code={`// Case 1: range of {key, value} pairs
std::vector<std::pair<std::string, int>> pairs = {
    {"one", 1}, {"two", 2}, {"three", 3}
};
auto m1 = imm::collect<std::string, int>(pairs.begin(), pairs.end());
// or:
auto m1b = imm::collect<std::string, int>(pairs);

// Case 2: range of {key, tuple<args...>} — Value constructed from tuple
struct Point { int x, y; Point(int a, int b) : x(a), y(b) {} };

std::vector<std::pair<std::string, std::tuple<int, int>>> data = {
    {"origin", {0, 0}},
    {"unit",   {1, 1}},
};
// Point is constructed as Point(0,0), Point(1,1)
auto m2 = imm::collect<std::string, Point>(data.begin(), data.end());
assert(m2.at("origin").x == 0);

// Case 3: explicit collect_construct for tuple ranges
auto m3 = imm::collect_construct<std::string, Point>(data);`}
          />
        </Section>

        {/* API */}
        <Section id="api" title="API Reference">
          <h3 className="text-lg font-semibold text-white mb-3">Template Parameters</h3>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-4 mb-6">
            <Param name="Key" type="typename" desc="Copyable, movable, hashable. Stored as const in pair." />
            <Param name="Value" type="typename" desc="No move/copy required; constructed in-place" />
            <Param name="Hash" type="= std::hash<Key>" desc="Hash functor. Must work with CompatibleKey too." />
            <Param name="KeyEq" type="= std::equal_to<Key>" desc="Equality functor. Must work with CompatibleKey." />
            <Param name="Alloc" type="= std::allocator<std::byte>" desc="Allocator for raw memory" />
          </div>

          <h3 className="text-lg font-semibold text-white mb-3">immutable_map</h3>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-4 mb-6">
            <Param name="size()" type="→ size_type" desc="Number of elements" />
            <Param name="empty()" type="→ bool" desc="True if size() == 0" />
            <Param name="find(key)" type="→ pair*" desc="Heterogeneous: template<CompatibleKey>. Returns pair pointer or nullptr." />
            <Param name="at(key)" type="→ Value&" desc="Heterogeneous. Returns reference to value; throws if not found." />
            <Param name="contains(key)" type="→ bool" desc="Heterogeneous. True if key is present." />
            <Param name="begin() / end()" type="→ iterator" desc="Forward iterator, returns pair<const Key, Value>&" />
            <Param name="cbegin() / cend()" type="→ const_iterator" desc="Const forward iterator" />
          </div>

          <h3 className="text-lg font-semibold text-white mb-3">immutable_map_builder</h3>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-4 mb-6">
            <Param name="reserve(n)" type="void" desc="Allocate memory for n elements (call once, before emplace)" />
            <Param name="emplace(key, args...)" type="void" desc="Construct pair in-place. Key forwarded, Value from args." />
            <Param name="build()" type="→ immutable_map" desc="Build hash index, transfer ownership. Builder becomes empty." />
            <Param name="size()" type="→ size_type" desc="Number of elements emplaced so far" />
          </div>

          <h3 className="text-lg font-semibold text-white mb-3">Free Functions</h3>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-4">
            <Param name="collect<K,V>(first, last)" type="→ immutable_map" desc="Build from iterator range. Auto-detects tuple values." />
            <Param name="collect<K,V>(range)" type="→ immutable_map" desc="Build from range object (has begin/end)" />
            <Param name="collect_construct<K,V>(first, last)" type="→ immutable_map" desc="Build from range of {key, tuple<args...>}" />
          </div>
        </Section>

        {/* Examples */}
        <Section id="examples" title="Complete Examples">
          <div className="space-y-6">
            <div>
              <h3 className="text-white font-semibold mb-2">Iteration with structured bindings</h3>
              <CodeBlock
                language="cpp"
                code={`for (auto&& [key, value] : map) {
    std::cout << key << " → " << value << "\\n";
    // key is const std::string&
    // value is int& (mutable!)
    value *= 2;  // OK
}

// Const iteration:
const auto& cmap = map;
for (auto&& [key, value] : cmap) {
    // value is const int&
    // value *= 2;  // ERROR
}

// Iterator is a real forward iterator:
auto it = map.begin();
auto it2 = it;   // copyable
++it;
assert(it != it2);`}
              />
            </div>
            <div>
              <h3 className="text-white font-semibold mb-2">Move and copy semantics</h3>
              <CodeBlock
                language="cpp"
                code={`auto m1 = builder.build();
auto m2 = std::move(m1);    // O(1), just pointer swap
assert(m1.empty());          // moved-from state
assert(m2.size() == 3);

// Copy (only if Value is copy-constructible):
auto m3 = m2;               // allocates new block, copies data
assert(m3.size() == m2.size());

// If Value is not copyable, copy is a compile error:
// imm::immutable_map<int, NonMovable> m4 = m3;  // ERROR`}
              />
            </div>
            <div>
              <h3 className="text-white font-semibold mb-2">Custom allocator</h3>
              <CodeBlock
                language="cpp"
                code={`template<typename T>
struct MyAlloc : std::allocator<T> {
    using std::allocator<T>::allocator;
    template<typename U>
    struct rebind { using other = MyAlloc<U>; };
};

imm::immutable_map<std::string, int,
                   std::hash<std::string>,
                   std::equal_to<std::string>,
                   MyAlloc<std::byte>> map;`}
              />
            </div>
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
        <p className="mt-1">Single allocation · Builder pattern · CRTP copy · Heterogeneous lookup · std::pair</p>
      </footer>
    </div>
  );
}
