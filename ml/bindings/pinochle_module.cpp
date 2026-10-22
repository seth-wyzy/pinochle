#include "Pin.h"
#include "card.h"

#include <algorithm>
#include <cstdint>

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {

py::tuple training_state(Pin& game, int player) {
    auto observation = py::array_t<float>(Pin::OBSERVATION_SIZE);
    auto action_mask = py::array_t<bool>(Pin::ACTION_SPACE_SIZE);
    game.write_training_observation(player, observation.mutable_data());
    std::fill_n(action_mask.mutable_data(), Pin::ACTION_SPACE_SIZE, false);

    for (const int action : game.legal_training_actions()) {
        if (action >= 0 && action < Pin::ACTION_SPACE_SIZE) {
            action_mask.mutable_data()[action] = true;
        }
    }

    const int phase = game.training_phase();
    const int current_player = game.training_current_player();
    const int trump = game.training_trump();
    const int us_points = game.training_us_points();
    const int them_points = game.training_them_points();
    const int bid = game.training_bid();
    const int bid_winner = game.training_bid_winner();

    return py::make_tuple(
        observation, action_mask, phase, current_player, trump,
        us_points, them_points, bid, bid_winner
    );
}

} // namespace

PYBIND11_MODULE(pinochle_cpp, module) {
    module.doc() = "Non-interactive Pinochle training interface";

    py::class_<card>(module, "Card")
        .def(py::init<int, int, int>(), py::arg("rank"), py::arg("suit"),
             py::arg("copy") = 0)
        .def_readonly("rank", &card::rank)
        .def_readonly("suit", &card::suit)
        .def_readonly("copy", &card::mult)
        .def("rank_name", &card::p_rank)
        .def("suit_name", &card::p_suit);

    py::class_<Pin>(module, "PinochleGame")
        .def(py::init<>())
        .def("reset", &Pin::reset_training, py::arg("seed") = 0)
        .def("training_state", &training_state, py::arg("player"))
        .def("legal_actions", &Pin::legal_training_actions)
        .def("step", [](Pin& game, int action) {
            const TrainingStep result = game.step_training(action);
            return py::make_tuple(result.reward, result.terminated);
        })
        .def_property_readonly("hand", &Pin::training_hand,
                               py::return_value_policy::reference_internal)
        .def("player_hand", &Pin::training_player_hand, py::arg("player"),
             py::return_value_policy::reference_internal)
        .def_property_readonly("trick", &Pin::training_trick,
                               py::return_value_policy::reference_internal)
        .def_property_readonly("played_cards", &Pin::training_played_cards,
                               py::return_value_policy::reference_internal)
        .def_property_readonly("trump", &Pin::training_trump)
        .def_property_readonly("phase", &Pin::training_phase)
        .def_property_readonly("current_player", &Pin::training_current_player)
        .def_property_readonly("dealer", &Pin::training_dealer)
        .def_property_readonly("bid", &Pin::training_bid)
        .def_property_readonly("bid_winner", &Pin::training_bid_winner)
        .def_property_readonly("us_points", &Pin::training_us_points)
        .def_property_readonly("them_points", &Pin::training_them_points)
        .def_property_readonly("us_meld", &Pin::training_us_meld)
        .def_property_readonly("them_meld", &Pin::training_them_meld)
        .def_property_readonly("us_trick_points", &Pin::training_us_trick_points)
        .def_property_readonly("them_trick_points", &Pin::training_them_trick_points)
        .def_property_readonly("us_score", &Pin::training_us_score)
        .def_property_readonly("them_score", &Pin::training_them_score)
        .def_property_readonly("active_bidders", &Pin::training_active_bidders);
}
