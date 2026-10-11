import { useState } from 'react';
import { Highlight, themes } from 'prism-react-renderer';
import cppCode from './immutable_map.hpp?raw';

// Sections for navigation
const sections = [
  { id: 'overview', label: 'Overview' },
  { id: 'design', label: 'Design' },
  { id: 'builder', label: 'Builder' },
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
            fontSize: '0.85rem',
            lineHeight: '1.6',
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
          <nav className="hidden md:flex gap-1">
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

      {/* Main content */}
      <main className="max-w-6xl mx-auto px-4 py-12">
        {/* Overview */}
        <Section id="overview" title="Overview">
          <p className="text-slate-300 leading-relaxed mb-4">
            <code className="text-emerald-400 bg-slate-800 px-1.5 py-0.5 rounded text-sm">immutable_map</code> — шаблонный
            хэш-контейнер на C++17, в котором <strong className="text-white">структура данных неизменяема</strong> после
            инициализации, но значения доступны для модификации. Ключи иммутабельны; значения — нет.
          </p>
          <div className="grid sm:grid-cols-2 gap-4 mt-6">
            {[
              { icon: '🔒', title: 'Immutable structure', desc: 'Keys and topology are fixed after build()' },
              { icon: '📦', title: 'Single allocation', desc: 'Keys, values, and hash index in one memory block' },
              { icon: '🏗️', title: 'Builder pattern', desc: 'reserve → emplace → build for zero-copy construction' },
              { icon: '🧩', title: 'Allocator-aware', desc: 'Works with any standard-compatible allocator' },
              { icon: '🚫', title: 'No move/copy for Value', desc: 'Values are constructed in-place and never moved' },
              { icon: '📐', title: 'SFINAE copy', desc: 'Copy ops enabled only when Value is copy-constructible' },
            ].map((item) => (
              <div
                key={item.title}
                className="bg-slate-900 border border-slate-800 rounded-lg p-4"
              >
                <div className="text-2xl mb-2">{item.icon}</div>
                <h3 className="text-white font-semibold text-sm mb-1">{item.title}</h3>
                <p className="text-slate-400 text-sm">{item.desc}</p>
              </div>
            ))}
          </div>
        </Section>

        {/* Design */}
        <Section id="design" title="Memory Layout & Design">
          <p className="text-slate-300 leading-relaxed mb-4">
            Контейнер выделяет <strong className="text-white">один блок памяти</strong> через аллокатор,
            размещая в нём три области с учётом выравнивания:
          </p>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-6 mb-6 font-mono text-sm">
            <div className="text-slate-400 mb-2">Single allocation layout:</div>
            <div className="flex flex-wrap gap-0">
              <div className="border border-emerald-600 bg-emerald-900/30 px-4 py-2 rounded-l">
                <span className="text-emerald-400">Key[N]</span>
              </div>
              <div className="border border-yellow-600 bg-yellow-900/20 px-3 py-2 border-l-0">
                <span className="text-yellow-500 text-xs">pad</span>
              </div>
              <div className="border border-sky-600 bg-sky-900/30 px-4 py-2 border-l-0">
                <span className="text-sky-400">Value[N]</span>
              </div>
              <div className="border border-yellow-600 bg-yellow-900/20 px-3 py-2 border-l-0">
                <span className="text-yellow-500 text-xs">pad</span>
              </div>
              <div className="border border-purple-600 bg-purple-900/30 px-4 py-2 rounded-r border-l-0">
                <span className="text-purple-400">uint32_t[M]</span>
              </div>
            </div>
            <div className="text-slate-500 mt-3 text-xs">
              M = next_power_of_2(2·N) — load factor ≤ 0.5, open addressing, linear probing
            </div>
          </div>
          <p className="text-slate-300 leading-relaxed mb-4">
            Размер индекса <code className="text-purple-400">M</code> вычисляется из числа элементов{' '}
            <code className="text-emerald-400">N</code> на этапе <code className="text-sky-400">reserve()</code>,
            поэтому вся память выделяется однократно. Перемещение контейнера — просто передача указателя.
          </p>
          <CodeBlock
            language="cpp"
            code={`// Key constraints:
//   - CopyConstructible & MoveConstructible
//   - std::hash<Key> is valid
//   - std::equal_to<Key> is valid
//
// Value constraints:
//   - NOT required to be MoveConstructible or CopyConstructible
//   - Constructed in-place via emplace()
//   - If CopyConstructible → immutable_map gains copy operations (SFINAE)`}
          />
        </Section>

        {/* Builder */}
        <Section id="builder" title="Builder Pattern">
          <p className="text-slate-300 leading-relaxed mb-4">
            Вставка элементов осуществляется через <code className="text-emerald-400">immutable_map_builder</code>:
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

// Keys are immutable:
// map.at("beta").first = "delta";  // ERROR: const Key&`}
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

        {/* Collect */}
        <Section id="collect" title="Collect from Range">
          <p className="text-slate-300 leading-relaxed mb-4">
            Функция <code className="text-emerald-400">collect</code> автоматически определяет тип
            диапазона и строит карту. Поддерживаются forward_iterator и выше.
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
            <Param name="Key" type="typename" desc="Copyable, movable, hashable, equality-comparable" />
            <Param name="Value" type="typename" desc="No move/copy required; constructed in-place" />
            <Param name="Hash" type="= std::hash<Key>" desc="Hash function object" />
            <Param name="KeyEq" type="= std::equal_to<Key>" desc="Key equality predicate" />
            <Param name="Alloc" type="= std::allocator<std::byte>" desc="Allocator for raw memory" />
          </div>

          <h3 className="text-lg font-semibold text-white mb-3">immutable_map Methods</h3>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-4 mb-6">
            <Param name="size()" type="→ size_type" desc="Number of elements" />
            <Param name="empty()" type="→ bool" desc="True if size() == 0" />
            <Param name="find(key)" type="→ Value* / const Value*" desc="Returns pointer to value or nullptr" />
            <Param name="at(key)" type="→ Value& / const Value&" desc="Returns reference; throws if not found" />
            <Param name="contains(key)" type="→ bool" desc="True if key is present" />
            <Param name="begin() / end()" type="→ iterator" desc="Forward iterators (proxy: const Key&, Value&)" />
            <Param name="cbegin() / cend()" type="→ const_iterator" desc="Const forward iterators" />
          </div>

          <h3 className="text-lg font-semibold text-white mb-3">immutable_map_builder Methods</h3>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-4 mb-6">
            <Param name="reserve(n)" type="void" desc="Allocate memory for n elements (call once)" />
            <Param name="emplace(key, args...)" type="void" desc="Construct key-value pair in-place" />
            <Param name="build()" type="→ immutable_map" desc="Build index, transfer ownership" />
            <Param name="size()" type="→ size_type" desc="Number of elements emplaced so far" />
          </div>

          <h3 className="text-lg font-semibold text-white mb-3">Free Functions</h3>
          <div className="bg-slate-900 border border-slate-800 rounded-lg p-4">
            <Param name="collect<K,V>(first, last)" type="→ immutable_map" desc="Build from iterator range (auto-detects tuple values)" />
            <Param name="collect<K,V>(range)" type="→ immutable_map" desc="Build from range object (has begin/end)" />
            <Param name="collect_construct<K,V>(first, last)" type="→ immutable_map" desc="Build from range of {key, tuple<args...>}" />
          </div>
        </Section>

        {/* Examples */}
        <Section id="examples" title="Complete Examples">
          <div className="space-y-6">
            <div>
              <h3 className="text-white font-semibold mb-2">Custom allocator</h3>
              <CodeBlock
                language="cpp"
                code={`// Using a custom allocator
template<typename T>
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
}`}
              />
            </div>
            <div>
              <h3 className="text-white font-semibold mb-2">Move semantics</h3>
              <CodeBlock
                language="cpp"
                code={`auto m1 = builder.build();
auto m2 = std::move(m1);    // O(1), just pointer swap
assert(m1.empty());          // moved-from state
assert(m2.size() == 3);

// Copy (only if Value is copy-constructible):
auto m3 = m2;               // allocates new block, copies data
assert(m3.size() == m2.size());`}
              />
            </div>
          </div>
        </Section>

        {/* Source */}
        <Section id="source" title="Full Source Code">
          <p className="text-slate-300 mb-4">
            Complete implementation in a single header file (~450 lines). No external dependencies beyond the C++17 standard library.
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

      {/* Footer */}
      <footer className="border-t border-slate-800 py-8 text-center text-slate-500 text-sm">
        <p>immutable_map — C++17 header-only immutable hash map</p>
        <p className="mt-1">Single allocation · Builder pattern · Allocator-aware · SFINAE copy</p>
      </footer>
    </div>
  );
}
