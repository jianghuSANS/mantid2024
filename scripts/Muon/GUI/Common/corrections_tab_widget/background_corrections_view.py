# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2021 ISIS Rutherford Appleton Laboratory UKRI,
#   NScD Oak Ridge National Laboratory, European Spallation Source,
#   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
# SPDX - License - Identifier: GPL - 3.0 +
from mantidqt.utils.qt import load_ui

from qtpy.QtCore import Qt
from qtpy.QtGui import QDoubleValidator, QPalette
from qtpy.QtWidgets import QLineEdit, QStyledItemDelegate, QStyleOptionViewItem, QTableWidgetItem, QWidget

ui_form, widget = load_ui(__file__, "background_corrections_view.ui")

RUN_COLUMN_INDEX = 0
GROUP_COLUMN_INDEX = 1
START_X_COLUMN_INDEX = 2
END_X_COLUMN_INDEX = 3
A0_COLUMN_INDEX = 4
A0_ERROR_COLUMN_INDEX = 5
STATUS_COLUMN_INDEX = 6


class DoubleItemDelegate(QStyledItemDelegate):
    """
    An item delegate that has a QDoubleValidator. The __init__ is the default QStyledItemDelegate __init__.
    """

    def createEditor(self, parent, option, index):
        line_edit = QLineEdit(parent)
        line_edit.setValidator(QDoubleValidator())
        return line_edit


class StatusItemDelegate(QStyledItemDelegate):
    """
    An item delegate for changing the text color of the correction status.
    """

    def paint(self, painter, options, index):
        new_options = QStyleOptionViewItem(options)
        text_color = self.get_text_color(index.data())
        new_options.palette.setColor(QPalette.Text, text_color)
        super(StatusItemDelegate, self).paint(painter, new_options, index)

    @staticmethod
    def get_text_color(status):
        if "success" in status:
            return Qt.green
        elif "skipped" in status:
            return Qt.red
        else:
            return Qt.black


class BackgroundCorrectionsView(widget, ui_form):
    """
    The BackgroundCorrectionsView contains the widgets allowing a Background correction.
    """

    def __init__(self, parent: QWidget = None):
        """Initializes the DeadTimeCorrectionsView."""
        super(BackgroundCorrectionsView, self).__init__(parent)
        self.setupUi(self)

        self._setup_corrections_table()
        self._selected_row: int = None
        self._selected_column: int = None
        self._selected_value: str = None

        self.set_background_correction_options_visible(False)

        self._handle_start_x_changed = None
        self._handle_end_x_changed = None

        # Disable the background mode combo box while background corrections is still in development
        self.mode_combo_box.setEnabled(False)

    def set_slot_for_mode_combo_box_changed(self, slot) -> None:
        """Connect the slot for the Background corrections mode combo box."""
        self.mode_combo_box.currentIndexChanged.connect(slot)

    def set_slot_for_select_function_combo_box_changed(self, slot) -> None:
        """Connect the slot for the Select Function combo box changing."""
        self.function_combo_box.currentIndexChanged.connect(slot)

    def set_slot_for_group_combo_box_changed(self, slot) -> None:
        """Connect the slot for the Group selector combo box changing."""
        self.group_combo_box.currentIndexChanged.connect(slot)

    def set_slot_for_show_all_runs(self, slot) -> None:
        """Connect the slot for the Show All Runs check box."""
        self.show_all_runs_checkbox.stateChanged.connect(slot)

    def set_slot_for_start_x_changed(self, slot) -> None:
        """Sets the slot for when a start x table cell is changed."""
        self._handle_start_x_changed = slot

    def set_slot_for_end_x_changed(self, slot) -> None:
        """Sets the slot for when a end x table cell is changed."""
        self._handle_end_x_changed = slot

    def set_background_correction_options_visible(self, visible: bool) -> None:
        """Sets the Background corrections widgets as being visible or hidden."""
        self.select_function_label.setVisible(visible)
        self.function_combo_box.setVisible(visible)
        self.show_data_for_label.setVisible(visible)
        self.group_combo_box.setVisible(visible)
        self.show_all_runs_checkbox.setVisible(visible)
        self.apply_table_changes_to_all_checkbox.setVisible(visible)
        self.correction_options_table.setVisible(visible)
        self.background_info_label.setVisible(not visible)

    @property
    def background_correction_mode(self) -> str:
        """Returns the currently selected background correction mode."""
        return str(self.mode_combo_box.currentText())

    @background_correction_mode.setter
    def background_correction_mode(self, mode: str) -> None:
        """Sets the currently selected background correction mode."""
        index = self.mode_combo_box.findText(mode)
        if index != -1:
            self.mode_combo_box.setCurrentIndex(index)

    @property
    def selected_function(self) -> str:
        """Returns the currently selected function in the combo box."""
        return str(self.function_combo_box.currentText())

    @selected_function.setter
    def selected_function(self, function: str) -> None:
        """Sets the currently selected function in the combo box."""
        index = self.function_combo_box.findText(function)
        if index != -1:
            self.function_combo_box.setCurrentIndex(index)

    def populate_group_selector(self, groups: list) -> None:
        """Populates the group selector combo box."""
        old_group = self.group_combo_box.currentText()

        self.group_combo_box.blockSignals(True)

        self.group_combo_box.clear()
        self.group_combo_box.addItems(["All"] + groups)

        new_index = self.group_combo_box.findText(old_group)
        self.group_combo_box.setCurrentIndex(new_index if new_index != -1 else 0)

        self.group_combo_box.blockSignals(False)

        # Signal is emitted manually in case the index has not changed (but the loaded dataset may be different)
        self.group_combo_box.currentIndexChanged.emit(new_index)

    @property
    def selected_group(self) -> str:
        """Returns the currently selected Group."""
        return str(self.group_combo_box.currentText())

    @property
    def show_all_runs(self) -> bool:
        """Returns true if all the runs should be shown."""
        return self.show_all_runs_checkbox.isChecked()

    def selected_run_and_group(self) -> tuple:
        """Returns the Run and Group that is in the same row as the selected table cell."""
        if self._selected_row is not None:
            run = self.correction_options_table.item(self._selected_row, RUN_COLUMN_INDEX).text()
            group = self.correction_options_table.item(self._selected_row, GROUP_COLUMN_INDEX).text()
            return [run], [group]
        else:
            raise RuntimeError("There is no selected run/group table row.")

    def apply_table_changes_to_all(self) -> bool:
        """Returns true if the changes made in the table should be made for all domains at once."""
        return self.apply_table_changes_to_all_checkbox.isChecked()

    def is_run_group_displayed(self, run: str, group: str) -> bool:
        """Returns true if a Run-Group is currently displayed in the data table."""
        return self._table_row_index_of(run, group) is not None

    def set_start_x(self, run: str, group: str, start_x: float) -> None:
        """Sets the Start X associated with the provided Run and Group."""
        self._set_table_item_value_for(run, group, START_X_COLUMN_INDEX, start_x)

    def start_x(self, run: str, group: str) -> float:
        """Returns the Start X associated with the provided Run and Group."""
        return float(self._table_item_value_for(run, group, START_X_COLUMN_INDEX))

    def set_end_x(self, run: str, group: str, end_x: float) -> None:
        """Sets the End X associated with the provided Run and Group."""
        self._set_table_item_value_for(run, group, END_X_COLUMN_INDEX, end_x)

    def end_x(self, run: str, group: str) -> float:
        """Returns the Start X associated with the provided Run and Group."""
        return float(self._table_item_value_for(run, group, END_X_COLUMN_INDEX))

    def selected_start_x(self) -> float:
        """Returns the Start X in the row that is selected."""
        if self._selected_row is not None:
            return float(self.correction_options_table.item(self._selected_row, START_X_COLUMN_INDEX).text())
        else:
            raise RuntimeError("There is no selected run/group table row.")

    def selected_end_x(self) -> float:
        """Returns the End X in the row that is selected."""
        if self._selected_row is not None:
            return float(self.correction_options_table.item(self._selected_row, END_X_COLUMN_INDEX).text())
        else:
            raise RuntimeError("There is no selected run/group table row.")

    def populate_corrections_table(self, runs: list, groups: list, start_xs: list, end_xs: list, backgrounds: list,
                                   background_errors: list, statuses: list) -> None:
        """Populates the background corrections table with the provided data."""
        self.correction_options_table.blockSignals(True)
        self.correction_options_table.setRowCount(0)
        for run, group, start_x, end_x, background, background_error, status in zip(runs, groups, start_xs, end_xs,
                                                                                    backgrounds, background_errors,
                                                                                    statuses):
            row = self.correction_options_table.rowCount()
            self.correction_options_table.insertRow(row)
            self.correction_options_table.setItem(row, RUN_COLUMN_INDEX, self._create_table_item(run, False))
            self.correction_options_table.setItem(row, GROUP_COLUMN_INDEX, self._create_table_item(group, False))
            self.correction_options_table.setItem(row, START_X_COLUMN_INDEX, self._create_double_table_item(start_x))
            self.correction_options_table.setItem(row, END_X_COLUMN_INDEX, self._create_double_table_item(end_x))
            self.correction_options_table.setItem(row, A0_COLUMN_INDEX,
                                                  self._create_double_table_item(background, enabled=False))
            self.correction_options_table.setItem(row, A0_ERROR_COLUMN_INDEX,
                                                  self._create_double_table_item(background_error, enabled=False))
            self.correction_options_table.setItem(row, STATUS_COLUMN_INDEX,
                                                  self._create_table_item(status, False, alignment=Qt.AlignVCenter))
        self.correction_options_table.blockSignals(False)
        self.correction_options_table.resizeColumnsToContents()

    def _setup_corrections_table(self) -> None:
        """Setup the correction options table to have a good layout."""
        self._setup_double_item_delegate(START_X_COLUMN_INDEX)
        self._setup_double_item_delegate(END_X_COLUMN_INDEX)
        self._setup_double_item_delegate(A0_COLUMN_INDEX)
        self._setup_double_item_delegate(A0_ERROR_COLUMN_INDEX)
        self.correction_options_table.setItemDelegateForColumn(STATUS_COLUMN_INDEX,
                                                               StatusItemDelegate(self.correction_options_table))

        self.correction_options_table.cellChanged.connect(lambda row, column:
                                                          self._on_corrections_table_cell_changed(row, column))
        self.correction_options_table.itemClicked.connect(lambda item: self._on_item_clicked(item))

    def _setup_double_item_delegate(self, column_index: int) -> None:
        """Sets an items delegate with a QDoubleValidator for the specified column index."""
        self.correction_options_table.setItemDelegateForColumn(column_index,
                                                               DoubleItemDelegate(self.correction_options_table))

    def _on_item_clicked(self, item: QTableWidgetItem) -> None:
        """Handles when a table cell is clicked."""
        self._selected_row = item.row()
        self._selected_column = item.column()
        self._selected_value = item.text()

    def _on_corrections_table_cell_changed(self, _: int, column: int) -> None:
        """Handles when a cells value is changed in the corrections table."""
        self._format_selection()

        if column == START_X_COLUMN_INDEX:
            self._handle_start_x_changed()
        elif column == END_X_COLUMN_INDEX:
            self._handle_end_x_changed()

    def _create_double_table_item(self, value: float, editable: bool = True, enabled: bool = True) -> QTableWidgetItem:
        """Creates a table item for the corrections options table from a float."""
        return self._create_table_item(self._float_to_str(value), editable, enabled)

    @staticmethod
    def _create_table_item(text: str, editable: bool = True, enabled: bool = True, alignment: int = Qt.AlignCenter) \
            -> QTableWidgetItem:
        """Creates a table item for the corrections options table from a string."""
        item = QTableWidgetItem(text)
        item.setTextAlignment(alignment)
        if not editable:
            item.setFlags(item.flags() ^ Qt.ItemIsEditable)
        if not enabled:
            item.setFlags(item.flags() ^ Qt.ItemIsEnabled)
        return item

    def _set_table_item_value_for(self, run: str, group: str, column_index: int, value: float) -> None:
        """Sets the End X associated with the provided Run and Group."""
        row_index = self._table_row_index_of(run, group)

        self.correction_options_table.blockSignals(True)
        self.correction_options_table.setItem(row_index, column_index, self._create_double_table_item(value))
        self.correction_options_table.blockSignals(False)

    def _table_item_value_for(self, run: str, group: str, column_index: int) -> str:
        """Returns the value in a table cell for the provided Run, Group and column index."""
        row_index = self._table_row_index_of(run, group)
        return self.correction_options_table.item(row_index, column_index).text() if row_index is not None else None

    def _table_row_index_of(self, run: str, group: str) -> int:
        """Returns the row index of the provided Run and Group."""
        for row_index in range(self.correction_options_table.rowCount()):
            row_run = self.correction_options_table.item(row_index, RUN_COLUMN_INDEX).text()
            row_group = self.correction_options_table.item(row_index, GROUP_COLUMN_INDEX).text()
            if row_run == run and row_group == group:
                return row_index
        return None

    def _format_selection(self) -> None:
        """Formats the selected cell to have the expected number of decimal places."""
        if self._selected_row is not None and self._selected_column is not None:
            value = float(self.correction_options_table.item(self._selected_row, self._selected_column).text())
            self._set_selected_value(value)

    def _set_selected_value(self, value: float) -> None:
        """Sets the currently selected cell to the float value provided."""
        if self._selected_row is not None and self._selected_column is not None:
            self.correction_options_table.blockSignals(True)
            self.correction_options_table.setItem(self._selected_row, self._selected_column,
                                                  self._create_double_table_item(value))
            self.correction_options_table.blockSignals(False)

    @staticmethod
    def _float_to_str(value: float) -> str:
        """Converts a float to a string with three decimal places."""
        return f"{value:.3f}"
