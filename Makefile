CXX      := g++
MOC      := /usr/lib/qt6/libexec/moc
RCC      := /usr/lib/qt6/libexec/rcc
TARGET   := pfvc

# pkg-config flags
PW_CFLAGS  := $(shell pkg-config --cflags libpipewire-0.3)
PW_LIBS    := $(shell pkg-config --libs   libpipewire-0.3)
QT_CFLAGS  := $(shell pkg-config --cflags Qt6Widgets Qt6Core)
QT_LIBS    := $(shell pkg-config --libs   Qt6Widgets Qt6Core)

CXXFLAGS := -std=c++17 -Wall -Wextra -O0 -g $(PW_CFLAGS) $(QT_CFLAGS) -I. -Imoc
LIBS     := $(PW_LIBS) $(QT_LIBS)

SRCS := main.cpp \
        MainWindow.cpp \
        StreamBlock.cpp \
        Config.cpp \
        PipeWireManager.cpp \
        SettingsDialog.cpp \
	PlaybackBlock.cpp

# MOC_HEADERS should contain any .h with the Q_OBJECT macro
MOC_HEADERS := MainWindow.h \
               StreamBlock.h \
               PipeWireManager.h \
               SettingsDialog.h \
               PlaybackBlock.h

MOC_SRCS    := $(addprefix moc/moc_,$(notdir $(MOC_HEADERS:.h=.cpp)))

# Resource file
RCC_SRC  := moc/qrc_resources.cpp

OBJS := $(SRCS:.cpp=.o) $(MOC_SRCS:.cpp=.o) $(RCC_SRC:.cpp=.o)

# Install paths — override with: make install PREFIX=/usr
PREFIX      := /usr/local
BINDIR      := $(PREFIX)/bin
ICONDIR     := /usr/share/icons/hicolor/scalable/apps
DESKTOPDIR  := /usr/share/applications

.PHONY: all clean install uninstall

all: moc_dir $(TARGET)

moc_dir:
	@mkdir -p moc

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LIBS)

# Compile regular sources
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Compile moc-generated sources
moc/%.o: moc/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Generate moc files
moc/moc_%.cpp: %.h
	$(MOC) $(QT_CFLAGS) $< -o $@

# Generate rcc file from resources.qrc
$(RCC_SRC): resources.qrc pfvc_logo.svg
	$(RCC) $< -o $@

# Header dependencies (simple)
main.o:       main.cpp MainWindow.h Config.h
MainWindow.o: MainWindow.cpp MainWindow.h StreamBlock.h Config.h
StreamBlock.o: StreamBlock.cpp StreamBlock.h Config.h
PlaybackBlock.o: PlaybackBlock.cpp PlaybackBlock.h Config.h
Config.o:     Config.cpp Config.h

moc/moc_MainWindow.o: moc/moc_MainWindow.cpp MainWindow.h
moc/moc_StreamBlock.o: moc/moc_StreamBlock.cpp StreamBlock.h
moc/moc_PlaybackBlock.o: moc/moc_PlaybackBlock.cpp PlaybackBlock.h

install: all
	@echo "Installing binary..."
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	@echo "Installing icon..."
	install -Dm644 pfvc_logo.svg $(DESTDIR)$(ICONDIR)/pfvc.svg
	@echo "Installing desktop entry..."
	install -Dm644 pfvc.desktop $(DESTDIR)$(DESKTOPDIR)/pfvc.desktop
	@echo "Updating icon cache..."
	-gtk-update-icon-cache /usr/share/icons/hicolor/ 2>/dev/null || true
	@echo "Done. You may need to log out and back in for the icon to appear."

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -f $(DESTDIR)$(ICONDIR)/pfvc.svg
	rm -f $(DESTDIR)$(DESKTOPDIR)/pfvc.desktop
	-gtk-update-icon-cache /usr/share/icons/hicolor/ 2>/dev/null || true
	@echo "Uninstalled."

clean:
	rm -f $(TARGET) *.o moc/*.o moc/moc_*.cpp moc/qrc_resources.cpp
