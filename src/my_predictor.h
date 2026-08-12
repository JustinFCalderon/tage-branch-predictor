// Our CBP-2 predictor. This is the file the framework's predict.cc includes,
// and the only place that touches the framework's types.
//
// It is deliberately thin. All the machinery lives in separately testable
// headers; this is the adapter that maps CBP-2's two virtual calls onto it.

#include "tage_predictor.h"

class my_update : public branch_update {
public:
    tage::Lookup lookup;
};

class my_predictor : public branch_predictor {
public:
    my_update   u;
    branch_info bi;

    branch_update *predict(branch_info &b) {
        bi = b;
        if (b.br_flags & BR_CONDITIONAL) {
            u.lookup = tage_.lookup(b.address);
            u.direction_prediction(tage_.predict(u.lookup));
        } else {
            // Unconditional branches are not scored. We predict taken and,
            // crucially, do not touch history: global history is a record of
            // CONDITIONAL outcomes only.
            u.direction_prediction(true);
        }
        u.target_prediction(0);
        return &u;
    }

    void update(branch_update *bu, bool taken, unsigned int /*target*/) {
        if (!(bi.br_flags & BR_CONDITIONAL)) return;
        my_update *mu = static_cast<my_update *>(bu);
        tage_.update(mu->lookup, taken);
        tage_.push_history(taken);
    }

private:
    tage::TagePredictor tage_;
};