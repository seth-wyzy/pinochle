#include "Pin.h"
#include "card.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

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
        .def("legal_actions", &Pin::legal_training_actions)
        .def("step", [](Pin& game, int action) {
            const TrainingStep result = game.step_training(action);
            return py::make_tuple(result.reward, result.terminated);
        })
        .def_property_readonly("hand", &Pin::training_hand,
                               py::return_value_policy::reference_internal)
        .def_property_readonly("trick", &Pin::training_trick,
                               py::return_value_policy::reference_internal)
        .def_property_readonly("trump", &Pin::training_trump)
        .def_property_readonly("phase", &Pin::training_phase)
        .def_property_readonly("current_player", &Pin::training_current_player)
        .def_property_readonly("us_points", &Pin::training_us_points)
        .def_property_readonly("them_points", &Pin::training_them_points);
}
