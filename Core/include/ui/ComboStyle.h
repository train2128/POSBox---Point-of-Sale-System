#ifndef COMBOSTYLE_H
#define COMBOSTYLE_H

class QComboBox;

namespace ComboStyle {

// Gives a combo box's drop-down list rounded corners and draws its rows: the
// chosen row with a dot, the row under the mouse with a soft highlight. Called
// for every combo box from the app-wide event filter in Animations.
void prepare(QComboBox* combo);

} // namespace ComboStyle

#endif // COMBOSTYLE_H
