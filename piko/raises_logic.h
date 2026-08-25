#pragma once

inline int selectedIncident(bool changed, int selected, int incidentCount) {
  return changed || selected >= incidentCount ? 0 : selected;
}

inline bool feedDismissed(unsigned long revision, unsigned long dismissedRevision) {
  return revision && revision == dismissedRevision;
}
