from datetime import datetime, timedelta
from typing import List, Tuple, cast, Optional
import colorsys
import json

from logger.parse_logs import parse_logs, LogSection, LogMessage

from enum import Enum


from datetime import datetime


def parse_spdlog_time(time_str: str) -> datetime:
    """
    Parse a spdlog-style timestamp string (e.g. "01:22:36.053622")
    and return a datetime object with today's date.
    """
    return datetime.strptime(time_str, "%H:%M:%S.%f")


class TimelineVisualizer:
    def __init__(
        self,
        root_section: LogSection,
        build_direction: str = "up",
        ndc_units_per_second: float = 0.5,
        use_custom_root_section_height: bool = True,
        custom_root_section_height: float = 0.01,
        base_timeline_position_y: float = 0,
        draw_timeline: bool = True,
        timeline_tick_width: float = 0.01,
        timeline_tick_height: float = 0.1,
        # NOTE: custom start time is of the form that the logger prints out for times
        custom_start_time: Optional[str] = None,
    ):
        self.root_section = root_section
        self.commands: List[str] = []
        # NOTE: possibly unused?
        self.used_text_areas: List[Tuple[float, float, float, float]] = []

        # Configurable parameters

        self.build_direction_factor = -1 if build_direction == "down" else 1
        self.ndc_units_per_second = ndc_units_per_second
        self.use_custom_root_section_height = use_custom_root_section_height
        self.custom_root_section_height = custom_root_section_height
        self.base_timeline_position_y = base_timeline_position_y
        self.draw_timeline = draw_timeline
        self.timeline_tick_width = timeline_tick_width
        self.timeline_tick_height = timeline_tick_height

        if custom_start_time is not None:
            self.start_time: datetime = parse_spdlog_time(custom_start_time)
        else:
            self.start_time: datetime = root_section.start_time

        self.end_time: datetime = root_section.end_time
        self.total_duration: float = (
            self.end_time - self.start_time
        ).total_seconds() * 1e6  # microseconds

    @classmethod
    def from_config(cls, root_section: "LogSection", config_path: str):
        """Factory method to create a TimelineVisualizer from a JSON config file.

        Note that the config file's syntax is just json with the key being the TimelineVisualizer attribute and the value the value you want to use.
        """
        with open(config_path, "r") as f:
            config = json.load(f)

        # only keep keys that match the constructor arguments
        valid_keys = {
            k: v for k, v in config.items() if k in cls.__init__.__code__.co_varnames
        }

        print(f"Calling {cls.__name__}  {valid_keys}")

        return cls(root_section, **valid_keys)

    # -------------------------------------------------
    # Geometry + layout helpers
    # -------------------------------------------------

    # NOTE: this is what determines the scale to a certain extent
    def time_to_x(self, timestamp: datetime) -> float:
        elapsed_seconds = (timestamp - self.start_time).total_seconds()
        ndc_x_pos = elapsed_seconds * self.ndc_units_per_second
        return ndc_x_pos

    # def time_to_x(self, timestamp: datetime) -> float:
    #     elapsed_microseconds = (timestamp - self.start_time).total_seconds() * 1e6
    #     return -1.0 + 2.0 * (elapsed_microseconds / self.total_duration)

    def generate_color(self, depth: int, index: int = 0) -> tuple:
        hue = (depth * 0.3 + index * 0.1) % 1.0
        saturation = 0.7
        value = 0.9 - (depth * 0.1) % 0.6
        return colorsys.hsv_to_rgb(hue, saturation, value)

    # -------------------------------------------------
    # Drawing methods
    # -------------------------------------------------
    def draw_base_timeline(self):
        self.commands.append(
            f"generate_rectangle(-0.0, {self.base_timeline_position_y}, 0.0, 2.0, 0.02) | (0.5, 0.5, 0.5)"
        )

    def draw_ticks(self):
        for i in range(11):
            x_pos = -1.0 + (i * 0.2)
            time_fraction = i / 10.0
            tick_time_us = time_fraction * self.total_duration
            tick_timestamp = self.start_time + timedelta(microseconds=tick_time_us)

            self.commands.append(
                f"generate_rectangle({x_pos:.3f}, {self.base_timeline_position_y}, 0.0, {self.timeline_tick_width}, {self.timeline_tick_height}) | (0.4, 0.4, 0.4)"
            )
            time_str = tick_timestamp.strftime("%H:%M:%S.%f")
            text_x, text_y = (
                x_pos - 0.05,
                self.base_timeline_position_y - 0.1 * self.build_direction_factor,
            )
            self.used_text_areas.append((text_x, text_x + 0.1, text_y, text_y + 0.08))
            self.commands.append(
                f'get_text_geometry("{time_str}", Rectangle(({text_x:.3f}, {text_y:.3f}, 0.1), 0.2, 0.16)) | (0.8, 0.8, 0.8)'
            )

    def get_depth_scale(self, depth: int) -> float:
        return 1 / (depth + 1)

    def get_event_width_depth_based(
        self, root_section_width: float, depth: int
    ) -> float:
        return 0.005 * self.get_depth_scale(depth + 1) * (root_section_width / 2)

    def get_event_width_aspect_ratio_based(self, section_width: float) -> float:
        result = section_width / 1000
        return max(result, 1e-6)  # or some other minimum threshold

    def get_section_rect_height_depth_based(
        self, root_section_width: float, depth: int
    ) -> float:
        return 0.15 * self.get_depth_scale(depth) * (root_section_width / 2)

    def get_section_rect_height_aspect_ratio_based(self, section_width: float) -> float:
        return section_width / 4

    def draw_section_rect(
        self,
        section: LogSection,
        depth: int,
        section_index: int,
        bottom_y_section: float,
        root_section_width: float,
        height_is_depth_based: float = True,
    ) -> tuple[float, float, float]:
        # NOTE: this ocmpotation is redundant and equals root_section_width, get rid of this soon
        x_start = self.time_to_x(section.start_time)
        x_end = self.time_to_x(section.end_time)
        # NOTE: at the base level width = 2
        width = x_end - x_start

        height: float
        if height_is_depth_based:
            height = self.get_section_rect_height_depth_based(root_section_width, depth)
        else:
            height = self.get_section_rect_height_aspect_ratio_based(width)

        if section.name == "root":
            if self.use_custom_root_section_height:
                height = self.custom_root_section_height

        rect_center_y = bottom_y_section + (height / 2) * self.build_direction_factor
        rect_center_x = (x_start + x_end) / 2

        color = self.generate_color(depth, section_index)

        # Section rectangle
        self.commands.append(
            f"generate_rectangle({rect_center_x:.6f}, {rect_center_y:.6f}, 0.0, {width:.6f}, {height:.6f}) "
            f"| ({color[0]:.3f}, {color[1]:.3f}, {color[2]:.3f})"
        )

        def format_duration_us(duration_us: float) -> str:
            units = [
                ("µs", 1),
                ("ms", 1_000),
                ("s", 1_000_000),
                ("min", 60 * 1_000_000),
                ("h", 3600 * 1_000_000),
            ]

            # Find the largest unit that keeps the value >= 1
            for unit, factor in reversed(units):
                if duration_us >= factor:
                    value = duration_us / factor
                    # 3 decimal places for small values, 0 if it's big
                    if value < 10:
                        return f"{value:.3f}{unit}"
                    elif value < 100:
                        return f"{value:.2f}{unit}"
                    else:
                        return f"{value:.0f}{unit}"
            return f"{duration_us:.0f}µs"  # fallback

        # Section text label with duration
        duration = section.duration_microseconds()
        duration_text = (
            f" ({format_duration_us(duration)})" if duration is not None else ""
        )
        label_text = (
            section.name if hasattr(section, "name") else f"Section {section_index}"
        ) + duration_text

        self.commands.append(
            f'get_text_geometry("{label_text}", Rectangle(({rect_center_x:.6f}, {rect_center_y:.6f}, 0.01), '
            f"{width:.6f}, {height:.6f})) | (0.9, 0.9, 0.9)"
        )

        return width, height, rect_center_y

    def group_event_sequences(self, section: "LogSection"):
        children_sorted = sorted(
            section.children,
            key=lambda c: c.timestamp if isinstance(c, LogMessage) else c.start_time,
        )
        event_sequences: List[List["LogMessage"]] = []
        current_seq: List["LogMessage"] = []

        for child in children_sorted:
            if isinstance(child, LogMessage):
                current_seq.append(child)
            else:
                if current_seq:
                    event_sequences.append(current_seq)
                    current_seq = []
        if current_seq:
            event_sequences.append(current_seq)
        return event_sequences

    def draw_event_sequence_annotations(
        self,
        event_sequence: List[LogMessage],
        parent_section: LogSection,
        depth: int,
        parent_section_rect_center_y: float,
        parent_section_rect_width: float,
        parent_section_rect_height: float,
        root_section_width: float,
        height_is_depth_based: float = True,
    ):
        first_event, last_event = event_sequence[0], event_sequence[-1]

        # Bounds limited by surrounding subsections
        sections_ending_before_first_event = [
            s
            for s in parent_section.children
            if isinstance(s, LogSection) and s.end_time <= first_event.timestamp
        ]
        left_bound_time = max(
            [s.end_time for s in sections_ending_before_first_event],
            default=parent_section.start_time,
        )

        sections_starting_after_last_event = [
            s
            for s in parent_section.children
            if isinstance(s, LogSection) and s.start_time >= last_event.timestamp
        ]
        right_bound_time = min(
            [s.start_time for s in sections_starting_after_last_event],
            default=parent_section.end_time,
        )

        # Rectangle for sequence
        availble_area_x_start = self.time_to_x(left_bound_time)
        available_area_x_end = self.time_to_x(right_bound_time)
        available_width = available_area_x_end - availble_area_x_start

        # NOTE: This needs to be updated

        num_events_in_sequence = len(event_sequence)

        # we're doing margin like THINGY|MARGIN, THINGY|MARGIN
        margin = 0.01 * available_width
        remaining_space = available_width - margin * (num_events_in_sequence - 1)
        annotation_rect_width = remaining_space / num_events_in_sequence

        parent_section_build_side_y = (
            parent_section_rect_center_y
            + (parent_section_rect_height / 2) * self.build_direction_factor
        )
        annotation_rect_center_y: float
        annotation_rect_height: float
        if height_is_depth_based:
            annotation_rect_center_y = (
                parent_section_build_side_y
                + (
                    self.get_section_rect_height_depth_based(root_section_width, depth)
                    * 1.5
                )
                * self.build_direction_factor
            )
            annotation_rect_height = (
                0.08 * self.get_depth_scale(depth) * (root_section_width / 2)
            )
        else:
            annotation_rect_center_y = (
                parent_section_build_side_y
                + (
                    self.get_section_rect_height_aspect_ratio_based(
                        parent_section_rect_width
                    )
                    * 1.5
                )
                * self.build_direction_factor
            )
            annotation_rect_height = annotation_rect_width / 4

        for i, event in enumerate(event_sequence):
            start_x = availble_area_x_start + i * (annotation_rect_width + margin)
            annotation_rect_center_x = start_x + annotation_rect_width / 2

            annotation_color = (0.4, 0.6, 0.8)
            self.commands.append(
                f"generate_rectangle({annotation_rect_center_x:.6f}, {annotation_rect_center_y:.6f}, 0.0, {annotation_rect_width:.6f}, {annotation_rect_height:.6f}) "
                f"| ({annotation_color[0]:.3f}, {annotation_color[1]:.3f}, {annotation_color[2]:.3f})"
            )

            # Label text
            label_text = event.message.strip()
            self.commands.append(
                f'get_text_geometry("{label_text}", Rectangle(({annotation_rect_center_x:.6f}, {annotation_rect_center_y:.6f}, 0.01), '
                f"{annotation_rect_width:.6f}, {annotation_rect_height:.6f})) | (1.0, 1.0, 0.8)"
            )

            # Connector line
            event_x = self.time_to_x(event.timestamp)

            # NOTE: we make sure this matches the annotations this syncro is kinda sketch rn
            event_y: float
            event_width: float

            if height_is_depth_based:
                event_y = (
                    parent_section_build_side_y
                    + self.get_section_rect_height_depth_based(
                        root_section_width, depth
                    )
                    * self.build_direction_factor
                )
                event_width = self.get_event_width_depth_based(
                    root_section_width, depth + 1
                )
            else:
                event_y = (
                    parent_section_build_side_y
                    + (parent_section_rect_height / 2) * self.build_direction_factor
                )
                event_width = self.get_event_width_aspect_ratio_based(
                    parent_section_rect_width
                )

            text_point_x = annotation_rect_center_x
            text_point_y = annotation_rect_center_y
            connector_color = (0.6, 0.6, 0.6)

            self.commands.append(
                f"generate_rectangle_between_2d(({text_point_x:.6f}, {text_point_y:.6f}), "
                f"({event_x:.6f}, {event_y:.6f}), {event_width:.6f}) | {connector_color}"
            )

    def process_section(
        self,
        section: LogSection,
        depth: int,
        bottom_y_current_section: float,
        section_index_for_labelling: int = 0,
        root_section_width: float = -1,
        height_is_depth_based: float = False,
    ):
        # NOTE: in this function we are constructing the next section on top of a previous section, bottom_y_current section is the base that we build up from
        # we are drawing a section, so we draw a rect, its events on top with its annotations and recurse on any other sections

        # why do we do this?
        if section.end_time is None:
            return bottom_y_current_section

        iterating_on_root = root_section_width == -1
        if iterating_on_root:
            x_start = self.time_to_x(section.start_time)
            x_end = self.time_to_x(section.end_time)
            # NOTE: at the base level width = 2 * scale
            root_section_width = x_end - x_start

        section_rect_width, section_rect_height, section_rect_center_y = (
            self.draw_section_rect(
                section,
                depth,
                section_index_for_labelling,
                bottom_y_current_section,
                root_section_width,
                height_is_depth_based,
            )
        )

        # Event annotations
        for seq in self.group_event_sequences(section):
            self.draw_event_sequence_annotations(
                seq,
                section,
                depth,
                section_rect_center_y,
                section_rect_width,
                section_rect_height,
                root_section_width,
                height_is_depth_based,
            )

        # iterate and recurse
        non_build_side_y_of_next_section = (
            bottom_y_current_section + section_rect_height * self.build_direction_factor
        )  # directly stack, no spacing
        child_section_index = 0

        for child in section.children:
            if isinstance(child, LogSection):
                self.process_section(
                    child,
                    depth + 1,
                    non_build_side_y_of_next_section,
                    child_section_index,
                    root_section_width,
                    height_is_depth_based,
                )
                child_section_index += 1
            elif isinstance(child, LogMessage):
                # NOTE: regular event drawing here.
                event_center_x = self.time_to_x(child.timestamp)
                event_color = (1.0, 0.8, 0.4)

                event_height: float
                event_width: float
                # NOTE: we make sure this matches the annotations this syncro is kinda sketch rn
                if height_is_depth_based:
                    event_height = self.get_section_rect_height_depth_based(
                        root_section_width, depth + 1
                    )
                    event_width = self.get_event_width_depth_based(
                        root_section_width, depth + 1
                    )
                else:
                    event_height = section_rect_height / 2
                    event_width = self.get_event_width_aspect_ratio_based(
                        section_rect_width
                    )

                build_side_y_of_current_section = (
                    bottom_y_current_section
                    + section_rect_height * self.build_direction_factor
                )
                event_center_y = (
                    build_side_y_of_current_section
                    + (event_height / 2) * self.build_direction_factor
                )
                self.commands.append(
                    f"generate_rectangle({event_center_x:.6f}, {event_center_y:.6f}, 0.0, {event_width:.6f}, {event_height:.6f}) "
                    f"| ({event_color[0]:.3f}, {event_color[1]:.3f}, {event_color[2]:.3f})"
                )

    # -------------------------------------------------
    # Public interface
    # -------------------------------------------------
    def generate(self) -> List[str]:
        self.commands.clear()
        self.used_text_areas.clear()

        if self.draw_timeline:
            self.draw_base_timeline()
            self.draw_ticks()

        self.process_section(
            self.root_section,
            0,
            self.base_timeline_position_y
            + (self.timeline_tick_height / 2 + 0.1) * self.build_direction_factor,
            0,
        )
        return self.commands

    def save(self, filename: str = "timeline_visualization.txt") -> List[str]:
        commands = self.generate()
        with open(filename, "w") as f:
            for cmd in commands:
                f.write(cmd + "\n")
        return commands


from typing import Callable, Optional
import importlib.util
import os


def load_user_transform(path: str) -> Optional[Callable[[str], str]]:
    """Load a user-defined transform function from a Python file.

    Returns:
        A callable (msg: str) -> str if successful, otherwise None.
    """
    if not os.path.isfile(path):
        return None

    try:
        spec = importlib.util.spec_from_file_location("log_message_transform", path)
        if spec is None or spec.loader is None:
            return None

        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    except Exception as e:
        print(f"Failed to load user transform from {path}: {e}")
        return None

    transform_fn = getattr(module, "transform", None)
    if callable(transform_fn):
        return cast(Callable[[str], str], transform_fn)

    print(f"No callable 'transform' found in {path}")
    return None


import argparse
import sys

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Parse a log file and generate timeline visualization commands."
    )
    parser.add_argument("log_file", help="Path to the log file to parse.")
    parser.add_argument(
        "--config",
        default=".parse_logs_config.json",
        help="Path to the visualization config file (default: .parse_logs_config.json)",
    )
    parser.add_argument(
        "--output",
        default="invocations.txt",
        help="Path to save the generated commands (default: invocations.txt)",
    )

    args = parser.parse_args()

    if not os.path.exists(args.log_file):
        print(f"Error: Log file '{args.log_file}' does not exist.", file=sys.stderr)
        sys.exit(1)

    user_transform = load_user_transform("log_message_transform.py")
    if user_transform:
        print("got custom transform")
    else:
        print("NO custom transform")

    root = parse_logs(args.log_file, user_transform)

    if os.path.exists(args.config):
        visualizer = TimelineVisualizer.from_config(root, args.config)
    else:
        print(f"Config file '{args.config}' not found, using default visualizer")
        visualizer = TimelineVisualizer(root)

    commands = visualizer.save(args.output)
    print(f"Generated {len(commands)} commands -> {args.output}")
