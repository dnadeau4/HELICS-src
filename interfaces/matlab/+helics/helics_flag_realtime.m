function v = helics_flag_realtime()
  persistent vInitialized;
  if isempty(vInitialized)
    vInitialized = helicsMEX(0, 32);
  end
  v = vInitialized;
end
