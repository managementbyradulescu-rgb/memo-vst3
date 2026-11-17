#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <map>
#include <vector>

// Transforma ce scrie omul ("vocal curat, cald, cu aer si putin delay")
// in setari pentru butoane. Ruleaza local, offline (fara internet / AI extern).
namespace memo
{
struct PromptResult
{
    std::vector<std::pair<juce::String, float>> values;   // id parametru -> valoare reala
    juce::StringArray understood;                          // ce am inteles (pentru afisare)
};

class PromptParser
{
public:
    static PromptResult parse (const juce::String& text)
    {
        PromptResult res;
        const auto tokens = tokenise (text);
        auto v = baseline();
        bool any = false;

        for (const auto& rule : rules())
        {
            int hit = -1;
            for (int i = 0; i < tokens.size() && hit < 0; ++i)
                for (const char* w : rule.words)
                    if (matches (tokens[i], w)) { hit = i; break; }

            if (hit < 0) continue;

            const float k = intensity (tokens, hit);
            for (const auto& d : rule.deltas)
                v[juce::String (d.id)] += d.amount * k;

            res.understood.add (rule.label);
            any = true;
        }

        if (! any) return res;

        for (auto& kv : v)
            res.values.emplace_back (kv.first, clampValue (kv.first, kv.second));

        return res;
    }

private:
    struct Delta { const char* id; float amount; };
    struct Rule  { const char* label; std::vector<const char*> words; std::vector<Delta> deltas; };

    static juce::String normalise (const juce::String& in)
    {
        juce::String out;
        for (auto ch : in.toLowerCase())
        {
            juce::juce_wchar r = ch;
            switch ((int) ch)
            {
                case 0x0103: case 0x00e2: r = 'a'; break;   // a-breve, a-circumflex
                case 0x00ee:              r = 'i'; break;
                case 0x0219: case 0x015f: r = 's'; break;
                case 0x021b: case 0x0163: r = 't'; break;
                default: break;
            }
            out += juce::String::charToString (r);
        }
        return out;
    }

    static juce::StringArray tokenise (const juce::String& text)
    {
        juce::StringArray t;
        t.addTokens (normalise (text), " ,.;:!?/()-\"'+&\n\t", "");
        t.removeEmptyStrings();
        return t;
    }

    // "=cuvant" = potrivire exacta, altfel "incepe cu"
    static bool matches (const juce::String& tok, const char* w)
    {
        const juce::String word (w);
        if (word.startsWithChar ('='))
            return tok == word.substring (1);
        return tok.startsWith (word);
    }

    static float intensity (const juce::StringArray& t, int idx)
    {
        float k = 1.f;
        for (int j = std::max (0, idx - 2); j < idx; ++j)
        {
            const auto& w = t[j];
            if (w.startsWith ("foarte") || w.startsWith ("mult") || w.startsWith ("very") || w.startsWith ("extra")
                || w.startsWith ("super") || w.startsWith ("maxim") || w.startsWith ("really") || w.startsWith ("mega"))
                k = 1.6f;
            else if (w.startsWith ("putin") || w.startsWith ("usor") || w.startsWith ("slight")
                     || w.startsWith ("little") || w == "bit" || w == "pic")
                k = 0.5f;
        }
        return k;
    }

    static std::map<juce::String, float> baseline()
    {
        return {
            { "noise", 0.35f }, { "upward", 0.30f }, { "peak", 0.40f }, { "reson", 0.35f },
            { "hpf", 100.f },   { "boxy", 0.35f },   { "clarity", 0.45f }, { "air", 0.35f },
            { "sat", 0.25f },   { "deess", 0.45f },  { "c1176", 0.40f }, { "la2a", 0.45f },
            { "dlytime", 250.f }, { "dlyfb", 0.25f }, { "dlymix", 0.f },
            { "revsize", 0.40f }, { "revmix", 0.f },  { "boost", 0.f }
        };
    }

    static float clampValue (const juce::String& id, float v)
    {
        if (id == "hpf")     return juce::jlimit (60.f, 200.f, v);
        if (id == "dlytime") return juce::jlimit (20.f, 1000.f, v);
        if (id == "dlyfb")   return juce::jlimit (0.f, 0.9f, v);
        if (id == "boost")   return juce::jlimit (0.f, 12.f, v);
        return juce::jlimit (0.f, 1.f, v);
    }

    static const std::vector<Rule>& rules()
    {
        static const std::vector<Rule> r = {
            { "curat & clar", { "curat", "clean", "clar", "transparent", "cristal", "crisp" },
              { { "clarity", 0.25f }, { "boxy", 0.20f }, { "reson", 0.20f }, { "noise", 0.15f } } },

            { "cald", { "cald", "warm", "gros", "full", "body", "dens" },
              { { "sat", 0.25f }, { "la2a", 0.20f }, { "air", -0.15f }, { "clarity", -0.10f } } },

            { "aer", { "aer", "air", "brill", "luminos", "sclipit", "sparkl", "shiny", "stralucit" },
              { { "air", 0.30f }, { "clarity", 0.15f } } },

            { "mai putine sibilante", { "sibil", "harsh", "ascut", "taios", "piercing", "deess", "=ess" },
              { { "deess", 0.30f }, { "clarity", -0.10f }, { "air", -0.10f } } },

            { "fara zgomot", { "zgomot", "noise", "fosnet", "hiss", "bzz", "buzz", "ventilat" },
              { { "noise", 0.30f } } },

            { "mai tare / prezent", { "=tare", "loud", "puternic", "prezent", "present", "forward", "=fata", "punch", "impact" },
              { { "upward", 0.20f }, { "peak", 0.10f }, { "c1176", 0.20f }, { "boost", 3.f } } },

            { "boost volum", { "incet", "quiet", "slab", "halk" },
              { { "boost", 4.f }, { "upward", 0.20f } } },

            { "compresie uniforma", { "compress", "uniform", "stabil", "controlat", "constant", "=even", "nivelat" },
              { { "peak", 0.20f }, { "la2a", 0.20f }, { "c1176", 0.10f }, { "upward", 0.10f } } },

            { "agresiv", { "agresiv", "aggress", "=dur", "grit", "trap", "drill", "=rap", "=hard", "energ" },
              { { "c1176", 0.30f }, { "sat", 0.20f }, { "air", 0.15f }, { "clarity", 0.10f }, { "boost", 2.f } } },

            { "soft & smooth", { "=pop", "moale", "soft", "smooth", "neted", "silk", "catifel", "delicat", "intim" },
              { { "la2a", 0.30f }, { "deess", 0.20f }, { "peak", 0.10f }, { "air", 0.05f } } },

            { "fara boxy", { "boxy", "cutie", "nazal", "nasal", "inchis", "muffled", "=mat", "=box" },
              { { "boxy", 0.35f }, { "reson", 0.20f }, { "clarity", 0.15f } } },

            { "mai plin", { "subtire", "thin" },
              { { "sat", 0.20f }, { "la2a", 0.15f }, { "boxy", -0.15f }, { "air", -0.10f } } },

            { "natural", { "natural", "organic", "discret" },
              { { "peak", -0.15f }, { "c1176", -0.15f }, { "la2a", -0.10f }, { "sat", -0.10f } } },

            { "delay", { "delay", "echo", "ecou" },
              { { "dlymix", 0.25f }, { "dlyfb", 0.10f } } },

            { "slap delay", { "slap" },
              { { "dlymix", 0.20f }, { "dlytime", -140.f }, { "dlyfb", -0.10f } } },

            { "reverb", { "reverb", "=hol", "hall", "spatios", "spatiu", "ambient", "atmosf", "sala", "biseric", "church", "plate", "=room" },
              { { "revmix", 0.25f }, { "revsize", 0.20f } } },

            { "reverb mare", { "=big", "huge", "imens", "=enorm" },
              { { "revsize", 0.30f }, { "revmix", 0.10f } } },

            { "uscat (fara efecte)", { "=uscat", "=dry" },
              { { "dlymix", -1.f }, { "revmix", -1.f } } }
        };
        return r;
    }
};
} // namespace memo
