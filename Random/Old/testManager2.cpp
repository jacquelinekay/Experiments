#include <SSVUtils/SSVUtils.hpp>

using Idx = std::size_t;
using Ctr = int;

template <typename>
class HManager;

namespace Internal
{
    template <typename T>
    class Uncertain
    {
    private:
        ssvu::AlignedStorageBasic<T> storage;

    public:
        template <typename... TArgs>
        inline void init(TArgs&&... mArgs) noexcept(
            ssvu::isNothrowConstructible<T>())
        {
            new(&storage) T(std::forward<TArgs>(mArgs)...);
        }
        inline void deinit() noexcept(ssvu::isNothrowDestructible<T>())
        {
            get().~T();
        }

        inline T& get() noexcept { return reinterpret_cast<T&>(storage); }
        inline const T& get() const noexcept
        {
            return reinterpret_cast<const T&>(storage);
        }
    };

    template <typename T>
    class Atom
    {
        template <typename>
        friend class ::HManager;

    private:
        Uncertain<T> data;
        Idx markIdx;
        bool alive{false};

        // Initializes the internal data
        template <typename... TArgs>
        inline void initData(TArgs&&... mArgs) noexcept(
            noexcept(data.init(std::forward<TArgs>(mArgs)...)))
        {
            SSVU_ASSERT(!alive);
            data.init(std::forward<TArgs>(mArgs)...);
        }

        // Deinitializes the internal data
        inline void deinitData() noexcept(noexcept(data.deinit()))
        {
            SSVU_ASSERT(!alive);
            data.deinit();
        }

    public:
        inline Atom() = default;
        inline Atom(Atom&&) = default;
        inline Atom& operator=(Atom&&) = default;

        inline T& getData() noexcept
        {
            SSVU_ASSERT(alive);
            return data.get();
        }
        inline const T& getData() const noexcept
        {
            SSVU_ASSERT(alive);
            return data.get();
        }
        inline void setDead() noexcept { alive = false; }

        // Disallow copies
        inline Atom(const Atom&) = delete;
        inline Atom& operator=(const Atom&) = delete;
    };
}

template <typename T>
class Handle
{
    template <typename>
    friend class HManager;

public:
    using AtomType = typename Internal::Atom<T>;

private:
    HManager<T>& manager;
    Idx markIdx;
    Ctr ctr;

    inline Handle(HManager<T>& mManager, Idx mMarkIdx, Ctr mCtr) noexcept
        : manager(mManager),
          markIdx{mMarkIdx},
          ctr{mCtr}
    {
    }

    template <typename TT>
    inline TT getAtomImpl() noexcept
    {
        SSVU_ASSERT(isAlive());
        return manager.getAtomFromMark(manager.marks[markIdx]);
    }

public:
    inline AtomType& getAtom() noexcept { return getAtomImpl<AtomType&>(); }
    inline const AtomType& getAtom() const noexcept
    {
        return getAtomImpl<const AtomType&>();
    }
    inline T& get() noexcept { return getAtom().getData(); }
    inline const T& get() const noexcept { return getAtom().getData(); }
    bool isAlive() const noexcept;
    void destroy() noexcept;

    inline T& operator*() noexcept { return get(); }
    inline const T& operator*() const noexcept { return get(); }
    inline T* operator->() noexcept { return &(get()); }
    inline const T* operator->() const noexcept { return &(get()); }
};

template <typename T>
class HManager
{
    template <typename>
    friend class Handle;

private:
    struct Mark
    {
        Idx atomIdx;
        Ctr ctr;
    };

public:
    using AtomType = typename Internal::Atom<T>;

private:
    std::vector<AtomType> atoms;
    std::vector<Mark> marks;
    Idx size{0u}, sizeNext{0u};

    inline std::size_t getCapacity() const noexcept { return atoms.size(); }

    inline void growCapacityBy(std::size_t mAmount)
    {
        auto i(getCapacity()), newCapacity(getCapacity() + mAmount);
        SSVU_ASSERT(newCapacity >= 0 && newCapacity >= getCapacity());

        atoms.resize(newCapacity);
        marks.resize(newCapacity);

        // Initialize resized storage
        for(; i < newCapacity; ++i) atoms[i].markIdx = marks[i].atomIdx = i;
    }

    inline void growCapacityTo(std::size_t mCapacity)
    {
        SSVU_ASSERT(getCapacity() < mCapacity)
        growCapacityBy(mCapacity - getCapacity());
    }

    inline void growIfNeeded()
    {
        constexpr float growMultiplier{2.f};
        constexpr std::size_t growAmount{5};

        if(getCapacity() <= sizeNext)
            growCapacityTo((getCapacity() + growAmount) * growMultiplier);
    }

    inline void destroy(Idx mMarkIdx) noexcept
    {
        getAtomFromMark(marks[mMarkIdx]).setDead();
    }

    inline Mark& getMarkFromAtom(const AtomType& mAtom) noexcept
    {
        return marks[mAtom.markIdx];
    }
    inline AtomType& getAtomFromMark(const Mark& mMark) noexcept
    {
        return atoms[mMark.atomIdx];
    }

    inline void cleanUpMemory()
    {
        refresh();
        for(auto i(0u); i < size; ++i)
        {
            SSVU_ASSERT(atoms[i].alive);
            atoms[i].alive = false;
            atoms[i].deinitData();
        }
    }

public:
    inline HManager() = default;
    inline ~HManager() { cleanUpMemory(); }

    inline void clear() noexcept
    {
        cleanUpMemory();
        atoms.clear();
        marks.clear();
        size = sizeNext = 0u;
    }

    inline void reserve(std::size_t mCapacity)
    {
        if(getCapacity() < mCapacity) growCapacityTo(mCapacity);
    }

    inline Handle<T> createHandleFromAtom(AtomType& mAtom) noexcept
    {
        return {*this, mAtom.markIdx, getMarkFromAtom(mAtom).ctr};
    }

    template <typename... TArgs>
    inline Handle<T> create(TArgs&&... mArgs)
    {
        // `sizeNext` may be greater than the sizes of the vectors - resize
        // vectors if needed
        growIfNeeded();

        // `sizeNext` now is the first empty valid index - we create our atom
        // there
        auto& atom(atoms[sizeNext]);
        atom.initData(std::forward<TArgs>(mArgs)...);
        atom.alive = true;

        // Update the mark
        auto& mark(getMarkFromAtom(atom));
        mark.atomIdx = sizeNext;
        ++mark.ctr;

        // Update next size
        ++sizeNext;

        return createHandleFromAtom(atom);
    }

    inline void refresh()
    {
        // Type must be signed, to check with negative values later
        int iAlive{0}, iDead{0};

        // Convert `sizeNext` to int, to avoid warnings/runtime errors
        const int intSizeNext{static_cast<int>(sizeNext)};

        // Find first alive and first dead atoms
        while(iDead < intSizeNext && atoms[iDead].alive) ++iDead;
        iAlive = iDead - 1;

        int iD{iDead};
        int lastAlive{iAlive};
        for(; iD < intSizeNext; ++iD)
        {
            // Skip alive atoms
            if(atoms[iD].alive)
            {
                lastAlive = iD;
                continue;
            }

            // Found a dead atom - `iD` now stores its index
            // Look for an alive atom after the dead atom
            for(int iA{iD + 1}; true; ++iA)
            {
                // No more alive atoms, end refresh
                if(iA == intSizeNext)
                {
                    lastAlive = iD - 1;
                    goto end;
                }

                if(atoms[iA].alive)
                {
                    // Found an alive atom after dead `i` atom
                    std::swap(atoms[iA], atoms[iD]);

                    // Update mark of alive atom
                    getMarkFromAtom(atoms[iD]).atomIdx = iD;

                    break;
                }
            }
        }

    end:

        for(int kk{lastAlive + 1}; kk < intSizeNext; ++kk)
        {
            // Clean up dead atom
            atoms[kk].deinitData();
            ++(getMarkFromAtom(atoms[kk]).ctr);
        }

        size = sizeNext = lastAlive + 1; // Update size
    }

    template <typename TFunc>
    inline void forEach(TFunc mFunc)
    {
        for(auto i(0u); i < size; ++i) mFunc(atoms[i].getData());
    }
    template <typename TFunc>
    inline void forEachAtom(TFunc mFunc)
    {
        for(auto i(0u); i < size; ++i) mFunc(atoms[i]);
    }

    inline AtomType& getAtomAt(Idx mIdx) noexcept
    {
        SSVU_ASSERT(mIdx < atoms.size());
        return atoms[mIdx];
    }
    inline const AtomType& getAtomAt(Idx mIdx) const noexcept
    {
        SSVU_ASSERT(mIdx < atoms.size());
        return atoms[mIdx];
    }
    inline T& getDataAt(Idx mIdx) noexcept { return getAtomAt(mIdx).getData(); }
    inline const T& getDataAt(Idx mIdx) const noexcept
    {
        return getAtomAt(mIdx).getData();
    }

    inline std::size_t getSize() const noexcept { return size; }
    inline std::size_t getSizeNext() const noexcept { return sizeNext; }
};

template <typename T>
inline bool Handle<T>::isAlive() const noexcept
{
    return manager.marks[markIdx].ctr == ctr;
}

template <typename T>
inline void Handle<T>::destroy() noexcept
{
    return manager.destroy(markIdx);
}

SSVUT_TEST(HandleManager)
{
    int cc{0}, dd{0};

    struct OTest
    {
        std::string s;
        int& rCC;
        int& rDD;

        OTest(int& mRCC, int& mRDD) : rCC(mRCC), rDD(mRDD) { ++rCC; }
        ~OTest() { ++rDD; }
    };

    HManager<OTest> mgr;

    for(int k = 0; k < 2; ++k)
    {
        cc = dd = 0;

        auto a0(mgr.create(cc, dd));
        auto a1(mgr.create(cc, dd));
        auto a2(mgr.create(cc, dd));
        auto a3(mgr.create(cc, dd));
        auto a4(mgr.create(cc, dd));
        auto a5(mgr.create(cc, dd));
        auto a6(mgr.create(cc, dd));

        SSVUT_EXPECT(cc == 7);
        ssvu::lo("BUG") << dd << std::endl;
        SSVUT_EXPECT(dd == 0);
        SSVUT_EXPECT(mgr.getSize() == 0);
        SSVUT_EXPECT(mgr.getSizeNext() == 7);

        mgr.refresh();

        SSVUT_EXPECT(cc == 7);
        ssvu::lo("BUG") << dd << std::endl;
        SSVUT_EXPECT(dd == 0);
        SSVUT_EXPECT(mgr.getSize() == 7);
        SSVUT_EXPECT(mgr.getSizeNext() == 7);

        a0->s = "hi";
        a4->s = "ciao";
        a6->s = "bye";

        a2.destroy();
        a3.destroy();
        a5.destroy();

        SSVUT_EXPECT(cc == 7);
        ssvu::lo("BUG") << dd << std::endl;
        SSVUT_EXPECT(dd == 0);
        SSVUT_EXPECT(mgr.getSize() == 7);
        SSVUT_EXPECT(mgr.getSizeNext() == 7);

        mgr.refresh();

        SSVUT_EXPECT(cc == 7);
        SSVUT_EXPECT(dd == 3);
        SSVUT_EXPECT(mgr.getSize() == 4);
        SSVUT_EXPECT(mgr.getSizeNext() == 4);

        SSVUT_EXPECT(a0->s == "hi");
        SSVUT_EXPECT(a4->s == "ciao");
        SSVUT_EXPECT(a6->s == "bye");

        SSVUT_EXPECT(a0.isAlive());
        SSVUT_EXPECT(a1.isAlive());
        SSVUT_EXPECT(!a2.isAlive());
        SSVUT_EXPECT(!a3.isAlive());
        SSVUT_EXPECT(a4.isAlive());
        SSVUT_EXPECT(!a5.isAlive());
        SSVUT_EXPECT(a6.isAlive());

        mgr.forEach([](OTest& mA)
            {
                mA.s += "bb";
            });

        SSVUT_EXPECT(a0->s == "hibb");
        SSVUT_EXPECT(a4->s == "ciaobb");
        SSVUT_EXPECT(a6->s == "byebb");

        SSVUT_EXPECT(a0.isAlive());
        SSVUT_EXPECT(a1.isAlive());
        SSVUT_EXPECT(!a2.isAlive());
        SSVUT_EXPECT(!a3.isAlive());
        SSVUT_EXPECT(a4.isAlive());
        SSVUT_EXPECT(!a5.isAlive());
        SSVUT_EXPECT(a6.isAlive());

        auto aNew(mgr.create(cc, dd));
        aNew->s = "hehe";

        SSVUT_EXPECT(cc == 8);
        SSVUT_EXPECT(dd == 3);
        SSVUT_EXPECT(mgr.getSize() == 4);
        SSVUT_EXPECT(mgr.getSizeNext() == 5);

        mgr.refresh();

        SSVUT_EXPECT(cc == 8);
        SSVUT_EXPECT(dd == 3);
        SSVUT_EXPECT(mgr.getSize() == 5);
        SSVUT_EXPECT(mgr.getSizeNext() == 5);

        SSVUT_EXPECT(a0.isAlive());
        SSVUT_EXPECT(a1.isAlive());
        SSVUT_EXPECT(!a2.isAlive());
        SSVUT_EXPECT(!a3.isAlive());
        SSVUT_EXPECT(a4.isAlive());
        SSVUT_EXPECT(!a5.isAlive());
        SSVUT_EXPECT(a6.isAlive());
        SSVUT_EXPECT(aNew.isAlive());

        SSVUT_EXPECT(aNew->s == "hehe");

        a0.destroy();
        mgr.refresh();

        SSVUT_EXPECT(!a0.isAlive());
        SSVUT_EXPECT(a1.isAlive());
        SSVUT_EXPECT(!a2.isAlive());
        SSVUT_EXPECT(!a3.isAlive());
        SSVUT_EXPECT(a4.isAlive());
        SSVUT_EXPECT(!a5.isAlive());
        SSVUT_EXPECT(a6.isAlive());
        SSVUT_EXPECT(aNew.isAlive());

        SSVUT_EXPECT(cc == 8);
        SSVUT_EXPECT(dd == 4);
        SSVUT_EXPECT(mgr.getSize() == 4);
        SSVUT_EXPECT(mgr.getSizeNext() == 4);

        auto aSuicide(mgr.create(cc, dd));

        SSVUT_EXPECT(cc == 9);
        SSVUT_EXPECT(dd == 4);
        SSVUT_EXPECT(mgr.getSize() == 4);
        SSVUT_EXPECT(mgr.getSizeNext() == 5);

        aSuicide.destroy();

        SSVUT_EXPECT(cc == 9);
        SSVUT_EXPECT(dd == 4);
        SSVUT_EXPECT(mgr.getSize() == 4);
        SSVUT_EXPECT(mgr.getSizeNext() == 5);

        mgr.refresh();

        SSVUT_EXPECT(cc == 9);
        SSVUT_EXPECT(dd == 5);
        SSVUT_EXPECT(mgr.getSize() == 4);
        SSVUT_EXPECT(mgr.getSizeNext() == 4);

        mgr.clear();

        SSVUT_EXPECT(cc == 9);
        SSVUT_EXPECT(dd == 9);
    }
}

volatile int state{0};
template <typename T>
struct OV : public T
{
    bool alive{true};
};

void doBench()
{
    using namespace ssvu;

    struct OSmall
    {
        char k[16];
        int myState;
        void a()
        {
            ++state;
            ++myState;
        }
        OSmall() { ++state; }
        ~OSmall() { ++state; }
    };
    struct OBig
    {
        char k[128];
        int myState;
        void a()
        {
            ++state;
            ++myState;
        }
        OBig() { ++state; }
        ~OBig() { ++state; }
    };

    constexpr std::size_t s(10000000);

    {
        {
            Benchmark::start("Vector - creation");
            std::vector<Uptr<OV<OSmall>>> storage;
            storage.reserve(s + 10000);
            Benchmark::endLo();

            Benchmark::start("Vector - filling");
            for(auto i(0u); i < s; ++i) storage.emplace_back(new OV<OSmall>);
            Benchmark::endLo();

            Benchmark::start("Vector - iteration");
            for(auto& o : storage) o->a();
            Benchmark::endLo();

            Benchmark::start("Vector - set dead");
            for(auto i(0u); i < s; i += 2) storage[i]->alive = false;
            Benchmark::endLo();

            Benchmark::start("Vector - refresh");
            eraseRemoveIf(storage, [](const Uptr<OV<OSmall>>& mO)
                {
                    return !mO->alive;
                });
            Benchmark::endLo();

            Benchmark::start("Vector - add/rem");
            for(auto k(0u); k < 3; ++k)
            {
                for(auto j(0u); j < 10000; ++j)
                {
                    auto& u(getEmplaceUptr<OV<OSmall>>(storage));
                    u.alive = false;
                }

                eraseRemoveIf(storage, [](const Uptr<OV<OSmall>>& mO)
                    {
                        return !mO->alive;
                    });
            }
            Benchmark::endLo();

            Benchmark::start("Vector - cleanup");
        }

        Benchmark::endLo();
    }

    ssvu::lo() << "" << std::endl;

    {
        {
            Benchmark::start("AtomM - creation");
            HManager<OSmall> storage;
            storage.reserve(s + 10000);
            Benchmark::endLo();

            Benchmark::start("AtomM - filling");
            for(auto i(0u); i < s; ++i) storage.create();
            Benchmark::endLo();

            Benchmark::start("AtomM - refresh");
            storage.refresh();
            Benchmark::endLo();

            Benchmark::start("AtomM - iteration");
            storage.forEach([](OSmall& mO)
                {
                    mO.a();
                });
            Benchmark::endLo();

            Benchmark::start("AtomM - set dead");
            storage.forEachAtom([](decltype(storage)::AtomType& mO)
                {
                    mO.setDead();
                });
            Benchmark::endLo();

            Benchmark::start("AtomM - refresh");
            storage.refresh();
            Benchmark::endLo();

            Benchmark::start("AtomM - add/rem");
            for(auto k(0u); k < 3; ++k)
            {
                for(auto j(0u); j < 10000; ++j)
                {
                    auto h(storage.create());
                    h.destroy();
                }

                storage.refresh();
            }
            Benchmark::endLo();

            Benchmark::start("AtomM - cleanup");
        }

        Benchmark::endLo();
    }

    ssvu::lo() << "" << std::endl;

    {
        {
            Benchmark::start("Vector - creation");
            std::vector<Uptr<OV<OBig>>> storage;
            storage.reserve(s + 10000);
            Benchmark::endLo();

            Benchmark::start("Vector - filling");
            for(auto i(0u); i < s; ++i) storage.emplace_back(new OV<OBig>);
            Benchmark::endLo();

            Benchmark::start("Vector - iteration");
            for(auto& o : storage) o->a();
            Benchmark::endLo();

            Benchmark::start("Vector - set dead");
            for(auto i(0u); i < s; i += 2) storage[i]->alive = false;
            Benchmark::endLo();

            Benchmark::start("Vector - refresh");
            eraseRemoveIf(storage, [](const Uptr<OV<OBig>>& mO)
                {
                    return !mO->alive;
                });
            Benchmark::endLo();

            Benchmark::start("Vector - add/rem");
            for(auto k(0u); k < 3; ++k)
            {
                for(auto j(0u); j < 10000; ++j)
                {
                    auto& u(getEmplaceUptr<OV<OBig>>(storage));
                    u.alive = false;
                }

                eraseRemoveIf(storage, [](const Uptr<OV<OBig>>& mO)
                    {
                        return !mO->alive;
                    });
            }
            Benchmark::endLo();

            Benchmark::start("Vector - cleanup");
        }

        Benchmark::endLo();
    }

    ssvu::lo() << "" << std::endl;

    {
        {
            Benchmark::start("AtomM - creation");
            HManager<OBig> storage;
            storage.reserve(s + 10000);
            Benchmark::endLo();

            Benchmark::start("AtomM - filling");
            for(auto i(0u); i < s; ++i) storage.create();
            Benchmark::endLo();

            Benchmark::start("AtomM - refresh");
            storage.refresh();
            Benchmark::endLo();

            Benchmark::start("AtomM - iteration");
            storage.forEach([](OBig& mO)
                {
                    mO.a();
                });
            Benchmark::endLo();

            Benchmark::start("AtomM - set dead");
            storage.forEachAtom([](decltype(storage)::AtomType& mO)
                {
                    mO.setDead();
                });
            Benchmark::endLo();

            Benchmark::start("AtomM - refresh");
            storage.refresh();
            Benchmark::endLo();

            Benchmark::start("AtomM - add/rem");
            for(auto k(0u); k < 3; ++k)
            {
                for(auto j(0u); j < 10000; ++j)
                {
                    auto h(storage.create());
                    h.destroy();
                }

                storage.refresh();
            }
            Benchmark::endLo();

            Benchmark::start("AtomM - cleanup");
        }

        Benchmark::endLo();
    }
}

int main()
{
    SSVUT_RUN();
    doBench();
    return 0;
}