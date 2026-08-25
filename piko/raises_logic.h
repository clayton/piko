#pragma once

inline int selectedIncident(bool changed, int selected, int incidentCount) {
  return changed || selected >= incidentCount ? 0 : selected;
}
