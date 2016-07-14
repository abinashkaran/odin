#include <iostream>
#include <unordered_map>

#include "proto/tripdirections.pb.h"
#include "proto/directions_options.pb.h"
#include "odin/enhancedtrippath.h"
#include "odin/directionsbuilder.h"
#include "odin/maneuversbuilder.h"
#include "odin/narrativebuilder.h"
#include "odin/narrative_builder_factory.h"

namespace {
// Minimum edge length
constexpr auto kMinEdgeLength = 0.0003f;

}

namespace valhalla {
namespace odin {

const std::unordered_map<int, TripDirections_VehicleType> translate_vehicle_type {
  { static_cast<int>(TripPath_VehicleType_kCar), TripDirections_VehicleType_kCar },
  { static_cast<int>(TripPath_VehicleType_kMotorcycle), TripDirections_VehicleType_kMotorcycle },
  { static_cast<int>(TripPath_VehicleType_kFourWheelDrive), TripDirections_VehicleType_kFourWheelDrive },
  { static_cast<int>(TripPath_VehicleType_kTractorTrailers), TripDirections_VehicleType_kTractorTrailers },
};

const std::unordered_map<int, TripDirections_PedestrianType> translate_pedestrian_type {
  { static_cast<int>(TripPath_PedestrianType_kFoot), TripDirections_PedestrianType_kFoot },
  { static_cast<int>(TripPath_PedestrianType_kWheelChair), TripDirections_PedestrianType_kWheelChair },
  { static_cast<int>(TripPath_PedestrianType_kSegway), TripDirections_PedestrianType_kSegway },
};

const std::unordered_map<int, TripDirections_BicycleType> translate_bicycle_type {
  { static_cast<int>(TripPath_BicycleType_kRoad), TripDirections_BicycleType_kRoad },
  { static_cast<int>(TripPath_BicycleType_kHybrid), TripDirections_BicycleType_kHybrid },
  { static_cast<int>(TripPath_BicycleType_kCity), TripDirections_BicycleType_kCity },
  { static_cast<int>(TripPath_BicycleType_kCross), TripDirections_BicycleType_kCross },
  { static_cast<int>(TripPath_BicycleType_kMountain), TripDirections_BicycleType_kMountain },
};

const std::unordered_map<int, TripDirections_TransitType> translate_transit_type {
  { static_cast<int>(TripPath_TransitType_kTram), TripDirections_TransitType_kTram },
  { static_cast<int>(TripPath_TransitType_kMetro), TripDirections_TransitType_kMetro },
  { static_cast<int>(TripPath_TransitType_kRail), TripDirections_TransitType_kRail },
  { static_cast<int>(TripPath_TransitType_kBus), TripDirections_TransitType_kBus },
  { static_cast<int>(TripPath_TransitType_kFerry), TripDirections_TransitType_kFerry },
  { static_cast<int>(TripPath_TransitType_kCableCar), TripDirections_TransitType_kCableCar },
  { static_cast<int>(TripPath_TransitType_kGondola), TripDirections_TransitType_kGondola },
  { static_cast<int>(TripPath_TransitType_kFunicular), TripDirections_TransitType_kFunicular },
};

const std::unordered_map<int, TripDirections_TravelMode> translate_travel_mode {
  { static_cast<int>(TripPath_TravelMode_kDrive), TripDirections_TravelMode_kDrive },
  { static_cast<int>(TripPath_TravelMode_kPedestrian), TripDirections_TravelMode_kPedestrian },
  { static_cast<int>(TripPath_TravelMode_kBicycle), TripDirections_TravelMode_kBicycle },
  { static_cast<int>(TripPath_TravelMode_kTransit), TripDirections_TravelMode_kTransit },
};

DirectionsBuilder::DirectionsBuilder() {
}

// Returns the trip directions based on the specified directions options
// and trip path. This method calls ManeuversBuilder::Build and
// NarrativeBuilder::Build to form the maneuver list. This method
// calls PopulateTripDirections to transform the maneuver list into the
// trip directions.
TripDirections DirectionsBuilder::Build(
    const DirectionsOptions& directions_options, TripPath& trip_path) {
  // Validate trip path node list
  if (trip_path.node_size() < 1) {
    throw std::runtime_error("Trip path does not have any nodes");
  }

  EnhancedTripPath* etp = static_cast<EnhancedTripPath*>(&trip_path);

  // Produce maneuvers and narrative if enabled
  std::list<Maneuver> maneuvers;
  if (directions_options.narrative()) {
    // Update the heading of ~0 length edges
    UpdateHeading(etp);

    // Create maneuvers
    ManeuversBuilder maneuversBuilder(directions_options, etp);
    maneuvers = maneuversBuilder.Build();

    // Create the narrative
    std::unique_ptr<NarrativeBuilder> narrative_builder =
        NarrativeBuilderFactory::Create(directions_options, etp);
    narrative_builder->Build(directions_options, etp, maneuvers);
  }

  // Return trip directions
  return PopulateTripDirections(directions_options, etp, maneuvers);
}

// Update the heading of ~0 length edges.
void DirectionsBuilder::UpdateHeading(EnhancedTripPath* etp) {
  for (size_t x = 0; x < etp->node_size(); ++x) {
    auto* prev_edge = etp->GetPrevEdge(x);
    auto* curr_edge = etp->GetCurrEdge(x);
    auto* next_edge = etp->GetNextEdge(x);
    if (curr_edge && (curr_edge->length() < kMinEdgeLength)) {

      // Set the current begin heading
      if (prev_edge && (prev_edge->length() >= kMinEdgeLength)) {
        curr_edge->set_begin_heading(prev_edge->end_heading());
      } else if (next_edge && (next_edge->length() >= kMinEdgeLength)) {
        curr_edge->set_begin_heading(next_edge->begin_heading());
      }

      // Set the current end heading
      if (next_edge && (next_edge->length() >= kMinEdgeLength)) {
        curr_edge->set_end_heading(next_edge->begin_heading());
      } else if (prev_edge && (prev_edge->length() >= kMinEdgeLength)) {
        curr_edge->set_end_heading(prev_edge->end_heading());
      }
    }
  }
}

// Returns the trip directions based on the specified directions options,
// trip path, and maneuver list.
TripDirections DirectionsBuilder::PopulateTripDirections(
    const DirectionsOptions& directions_options, EnhancedTripPath* etp,
    std::list<Maneuver>& maneuvers) {
  TripDirections trip_directions;

  // Populate trip and leg IDs
  trip_directions.set_trip_id(etp->trip_id());
  trip_directions.set_leg_id(etp->leg_id());
  trip_directions.set_leg_count(etp->leg_count());

  // Populate locations
  for (const auto& path_location : etp->location()) {
    auto* direction_location = trip_directions.add_location();
    direction_location->mutable_ll()->set_lat(path_location.ll().lat());
    direction_location->mutable_ll()->set_lng(path_location.ll().lng());
    if (path_location.type() == TripPath_Location_Type_kThrough) {
      direction_location->set_type(TripDirections_Location_Type_kThrough);
    } else {
      direction_location->set_type(TripDirections_Location_Type_kBreak);
    }

    if (path_location.has_heading())
      direction_location->set_heading(path_location.heading());
    if (path_location.has_name())
      direction_location->set_name(path_location.name());
    if (path_location.has_street())
      direction_location->set_street(path_location.street());
    if (path_location.has_city())
      direction_location->set_city(path_location.city());
    if (path_location.has_state())
      direction_location->set_state(path_location.state());
    if (path_location.has_postal_code())
      direction_location->set_postal_code(path_location.postal_code());
    if (path_location.has_country())
      direction_location->set_country(path_location.country());
    if (path_location.has_date_time())
      direction_location->set_date_time(path_location.date_time());
    if (path_location.has_side_of_street()) {
      if (path_location.side_of_street()
          == TripPath_Location_SideOfStreet_kLeft) {
        direction_location->set_side_of_street(
            TripDirections_Location_SideOfStreet_kLeft);
      } else if (path_location.side_of_street()
          == TripPath_Location_SideOfStreet_kRight) {
        direction_location->set_side_of_street(
            TripDirections_Location_SideOfStreet_kRight);
      } else {
        direction_location->set_side_of_street(
            TripDirections_Location_SideOfStreet_kNone);
      }
    }
  }

  // Populate maneuvers
  for (const auto& maneuver : maneuvers) {
    auto* trip_maneuver = trip_directions.add_maneuver();
    trip_maneuver->set_type(maneuver.type());
    trip_maneuver->set_text_instruction(maneuver.instruction());

    // Set street names
    for (const auto& street_name : maneuver.street_names()) {
      trip_maneuver->add_street_name(street_name->value());
    }

    // Set begin street names
    for (const auto& begin_street_name : maneuver.begin_street_names()) {
      trip_maneuver->add_begin_street_name(begin_street_name->value());
    }

    trip_maneuver->set_length(maneuver.length(directions_options.units()));
    trip_maneuver->set_time(maneuver.time());
    trip_maneuver->set_begin_cardinal_direction(
        maneuver.begin_cardinal_direction());
    trip_maneuver->set_begin_heading(maneuver.begin_heading());
    trip_maneuver->set_begin_shape_index(maneuver.begin_shape_index());
    trip_maneuver->set_end_shape_index(maneuver.end_shape_index());
    if (maneuver.portions_toll())
      trip_maneuver->set_portions_toll(maneuver.portions_toll());
    if (maneuver.portions_unpaved())
      trip_maneuver->set_portions_unpaved(maneuver.portions_unpaved());

    if (maneuver.HasVerbalTransitionAlertInstruction()) {
      trip_maneuver->set_verbal_transition_alert_instruction(
          maneuver.verbal_transition_alert_instruction());
    }

    if (maneuver.HasVerbalPreTransitionInstruction()) {
      trip_maneuver->set_verbal_pre_transition_instruction(
          maneuver.verbal_pre_transition_instruction());
    }

    if (maneuver.HasVerbalPostTransitionInstruction()) {
      trip_maneuver->set_verbal_post_transition_instruction(
          maneuver.verbal_post_transition_instruction());
    }

    // Populate sign information
    if (maneuver.HasExitSign()) {
      auto* trip_sign = trip_maneuver->mutable_sign();

      // Process exit number info
      if (maneuver.HasExitNumberSign()) {
        auto* trip_exit_number_elements = trip_sign
            ->mutable_exit_number_elements();
        for (const auto& exit_number : maneuver.signs().exit_number_list()) {
          auto* trip_exit_number_element = trip_exit_number_elements->Add();
          trip_exit_number_element->set_text(exit_number.text());
          trip_exit_number_element->set_consecutive_count(
              exit_number.consecutive_count());
        }
      }

      // Process exit branch info
      if (maneuver.HasExitBranchSign()) {
        auto* trip_exit_branch_elements = trip_sign
            ->mutable_exit_branch_elements();
        for (const auto& exit_branch : maneuver.signs().exit_branch_list()) {
          auto* trip_exit_branch_element = trip_exit_branch_elements->Add();
          trip_exit_branch_element->set_text(exit_branch.text());
          trip_exit_branch_element->set_consecutive_count(
              exit_branch.consecutive_count());
        }
      }

      // Process exit toward info
      if (maneuver.HasExitTowardSign()) {
        auto* trip_exit_toward_elements = trip_sign
            ->mutable_exit_toward_elements();
        for (const auto& exit_toward : maneuver.signs().exit_toward_list()) {
          auto* trip_exit_toward_element = trip_exit_toward_elements->Add();
          trip_exit_toward_element->set_text(exit_toward.text());
          trip_exit_toward_element->set_consecutive_count(
              exit_toward.consecutive_count());
        }
      }

      // Process exit name info
      if (maneuver.HasExitNameSign()) {
        auto* trip_exit_name_elements = trip_sign->mutable_exit_name_elements();
        for (const auto& exit_name : maneuver.signs().exit_name_list()) {
          auto* trip_exit_name_element = trip_exit_name_elements->Add();
          trip_exit_name_element->set_text(exit_name.text());
          trip_exit_name_element->set_consecutive_count(
              exit_name.consecutive_count());
        }
      }

    }

    // Roundabout exit count
    if (maneuver.roundabout_exit_count() > 0) {
      trip_maneuver->set_roundabout_exit_count(
          maneuver.roundabout_exit_count());
    }

    // Depart instructions
    if (!maneuver.depart_instruction().empty()) {
      trip_maneuver->set_depart_instruction(maneuver.depart_instruction());
    }
    if (!maneuver.verbal_depart_instruction().empty()) {
      trip_maneuver->set_verbal_depart_instruction(
          maneuver.verbal_depart_instruction());
    }

    // Arrive instructions
    if (!maneuver.arrive_instruction().empty()) {
      trip_maneuver->set_arrive_instruction(maneuver.arrive_instruction());
    }
    if (!maneuver.verbal_arrive_instruction().empty()) {
      trip_maneuver->set_verbal_arrive_instruction(
          maneuver.verbal_arrive_instruction());
    }

    // Process transit route
    if (maneuver.IsTransit()) {
      const auto& transit_route = maneuver.transit_info();
      auto* trip_transit_info = trip_maneuver->mutable_transit_info();
      if (!transit_route.onestop_id.empty()) {
        trip_transit_info->set_onestop_id(transit_route.onestop_id);
      }
      if (!transit_route.short_name.empty()) {
        trip_transit_info->set_short_name(transit_route.short_name);
      }
      if (!transit_route.long_name.empty()) {
        trip_transit_info->set_long_name(transit_route.long_name);
      }
      if (!transit_route.headsign.empty()) {
        trip_transit_info->set_headsign(transit_route.headsign);
      }
      trip_transit_info->set_color(transit_route.color);
      trip_transit_info->set_text_color(transit_route.text_color);
      if (!transit_route.description.empty()) {
        trip_transit_info->set_description(transit_route.description);
      }
      if (!transit_route.operator_onestop_id.empty()) {
        trip_transit_info->set_operator_onestop_id(
            transit_route.operator_onestop_id);
      }
      if (!transit_route.operator_name.empty()) {
        trip_transit_info->set_operator_name(transit_route.operator_name);
      }
      if (!transit_route.operator_url.empty()) {
        trip_transit_info->set_operator_url(transit_route.operator_url);
      }

      // Process transit stops
      for (auto& transit_stop : transit_route.transit_stops) {
        auto* trip_transit_stop = trip_transit_info->add_transit_stops();
        trip_transit_stop->set_type(transit_stop.type);
        if (!transit_stop.onestop_id.empty()) {
          trip_transit_stop->set_onestop_id(transit_stop.onestop_id);
        }
        if (!transit_stop.name.empty()) {
          trip_transit_stop->set_name(transit_stop.name);
        }
        if (!transit_stop.arrival_date_time.empty()) {
          trip_transit_stop->set_arrival_date_time(
              transit_stop.arrival_date_time);
        }
        if (!transit_stop.departure_date_time.empty()) {
          trip_transit_stop->set_departure_date_time(
              transit_stop.departure_date_time);
        }
        if (transit_stop.is_parent_stop) {
          trip_transit_stop->set_is_parent_stop(true);
        }
        if (transit_stop.assumed_schedule) {
          trip_transit_stop->set_assumed_schedule(true);
        }
        trip_transit_stop->mutable_ll()->set_lat(transit_stop.ll.lat());
        trip_transit_stop->mutable_ll()->set_lng(transit_stop.ll.lng());
      }
    }

    // Verbal multi-cue
    if (maneuver.verbal_multi_cue())
      trip_maneuver->set_verbal_multi_cue(maneuver.verbal_multi_cue());

    // Travel mode
    trip_maneuver->set_travel_mode(translate_travel_mode.find(maneuver.travel_mode())->second);

    // Travel type
    switch (maneuver.travel_mode()) {
      case TripPath_TravelMode_kDrive: {
        trip_maneuver->set_vehicle_type(
            translate_vehicle_type.find(maneuver.vehicle_type())->second);
        break;
      }
      case TripPath_TravelMode_kPedestrian: {
        trip_maneuver->set_pedestrian_type(
            translate_pedestrian_type.find(maneuver.pedestrian_type())->second);
        break;
      }
      case TripPath_TravelMode_kBicycle: {
        trip_maneuver->set_bicycle_type(
            translate_bicycle_type.find(maneuver.bicycle_type())->second);
        break;
      }
      case TripPath_TravelMode_kTransit: {
        trip_maneuver->set_transit_type(
            translate_transit_type.find(maneuver.transit_type())->second);
        break;
      }
    }
  }

  // Populate summary
  trip_directions.mutable_summary()->set_length(
      etp->GetLength(directions_options.units()));
  trip_directions.mutable_summary()->set_time(
      etp->node(etp->GetLastNodeIndex()).elapsed_time());

  // Populate shape
  trip_directions.set_shape(etp->shape());

  return trip_directions;
}

}
}
